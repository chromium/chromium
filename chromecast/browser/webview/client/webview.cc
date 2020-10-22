// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/client/webview.h"

#include <grpcpp/create_channel.h>

#include "base/bind.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gl/gl_bindings.h"

namespace chromecast {
namespace client {

namespace {

constexpr int kGrpcMaxReconnectBackoffMs = 1000;

constexpr char kBackCommand[] = "back";
constexpr char kCreateCommand[] = "create";
constexpr char kDestroyCommand[] = "destroy";
constexpr char kForwardCommand[] = "forward";
constexpr char kListCommand[] = "list";
constexpr char kNavigateCommand[] = "navigate";
constexpr char kResizeCommand[] = "resize";
constexpr char kPositionCommand[] = "position";
constexpr char kKeyCommand[] = "key";

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  WebviewClient* webview_client = static_cast<WebviewClient*>(data);
  if (webview_client->HasAvailableBuffer())
    webview_client->SchedulePaint();
}

void BufferReleaseCallback(void* data, wl_buffer* /* buffer */) {
  WebviewClient::BufferCallback* buffer_callback =
      static_cast<WebviewClient::BufferCallback*>(data);
  buffer_callback->buffer->busy = false;
  buffer_callback->client->SchedulePaint();
}

}  // namespace

using chromecast::webview::InputEvent;
using chromecast::webview::KeyInput;
using chromecast::webview::TouchInput;
using chromecast::webview::WebviewRequest;
using chromecast::webview::WebviewResponse;

WebviewClient::Webview::Webview() {}

WebviewClient::Webview::~Webview() {}

WebviewClient::WebviewClient()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      file_descriptor_watcher_(task_runner_) {}

WebviewClient::~WebviewClient() {}

bool WebviewClient::HasAvailableBuffer() {
  auto buffer_it =
      std::find_if(buffers_.begin(), buffers_.end(),
                   [](const std::unique_ptr<ClientBase::Buffer>& buffer) {
                     return !buffer->busy;
                   });
  return buffer_it != buffers_.end();
}

void WebviewClient::Run(const InitParams& params,
                        const std::string& channel_directory) {
  drm_format_ = params.drm_format;
  bo_usage_ = params.bo_usage;

  // Roundtrip to wait for display configuration.
  wl_display_roundtrip(display_.get());

  AllocateBuffers(params);

  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, kGrpcMaxReconnectBackoffMs);
  stub_ = chromecast::webview::PlatformViewsService::NewStub(
      ::grpc::CreateCustomChannel(std::string("unix:" + channel_directory),
                                  ::grpc::InsecureChannelCredentials(), args));

  std::cout << "Enter command: ";
  std::cout.flush();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebviewClient::Paint, base::Unretained(this)));

  stdin_controller_ = file_descriptor_watcher_.WatchReadable(
      STDIN_FILENO, base::BindRepeating(&WebviewClient::InputCallback,
                                        base::Unretained(this)));
  TakeExclusiveAccess();
  wl_display_controller_ = file_descriptor_watcher_.WatchReadable(
      wl_display_get_fd(display_.get()),
      base::BindRepeating(&WebviewClient::WlDisplayCallback,
                          base::Unretained(this)));
  run_loop_.Run();
}

void WebviewClient::AllocateBuffers(const InitParams& params) {
  static wl_buffer_listener buffer_listener = {BufferReleaseCallback};
  for (size_t i = 0; i < params.num_buffers; ++i) {
    auto buffer_callback = std::make_unique<BufferCallback>();
    auto buffer = CreateBuffer(size_, params.drm_format, params.bo_usage,
                               &buffer_listener, buffer_callback.get());
    if (!buffer) {
      LOG(ERROR) << "Failed to create buffer";
      return;
    }
    buffer_callback->client = this;
    buffer_callback->buffer = buffer.get();
    buffer_callbacks_.push_back(std::move(buffer_callback));
    buffers_.push_back(std::move(buffer));
  }
}

void WebviewClient::CreateWebview(const std::vector<std::string>& tokens) {
  if (tokens.size() != 2) {
    LOG(ERROR) << "Usage: create [ID]";
    return;
  }

  int id;
  if (!base::StringToInt(tokens[1], &id)) {
    LOG(ERROR) << "ID is not an int";
    return;
  } else if (webviews_.find(id) != webviews_.end()) {
    LOG(ERROR) << "Webview with ID " << tokens[1] << " already exists";
    return;
  }

  std::unique_ptr<Webview> webview = std::make_unique<Webview>();

  webview->buffer = CreateBuffer(gfx::Size(1, 1), drm_format_, bo_usage_);

  webview->surface.reset(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));

  webview->context = std::make_unique<::grpc::ClientContext>();
  webview->client = stub_->CreateWebview(webview->context.get());

  WebviewRequest request;
  request.mutable_create()->set_webview_id(id);
  request.mutable_create()->set_window_id(id);
  if (!webview->client->Write(request)) {
    LOG(ERROR) << ("Failed to create webview");
    return;
  }

  WebviewResponse response;
  if (!webview->client->Read(&response)) {
    LOG(ERROR) << "Failed to read webview creation response";
    return;
  }

  webview->subsurface.reset(wl_subcompositor_get_subsurface(
      globals_.subcompositor.get(), webview->surface.get(), surface_.get()));
  wl_subsurface_set_sync(webview->subsurface.get());

  std::unique_ptr<zaura_surface> aura_surface;
  aura_surface.reset(zaura_shell_get_aura_surface(globals_.aura_shell.get(),
                                                  webview->surface.get()));
  if (!aura_surface) {
    LOG(ERROR) << "No aura surface";
    return;
  }
  zaura_surface_set_client_surface_id(aura_surface.get(), id);

  webviews_[id] = std::move(webview);
}

void WebviewClient::DestroyWebview(const std::vector<std::string>& tokens) {
  int id;
  if (tokens.size() != 2 || !base::StringToInt(tokens[1], &id)) {
    LOG(ERROR) << "Usage: destroy [ID]";
    return;
  }
  webviews_.erase(id);
}

void WebviewClient::HandleDown(void* data,
                               struct wl_touch* wl_touch,
                               uint32_t serial,
                               uint32_t time,
                               struct wl_surface* surface,
                               int32_t id,
                               wl_fixed_t x,
                               wl_fixed_t y) {
  gfx::Point touch_point(wl_fixed_to_int(x), wl_fixed_to_int(y));

  auto iter = std::find_if(
      webviews_.begin(), webviews_.end(),
      [surface](const std::pair<const int, std::unique_ptr<Webview>>& pair) {
        const auto& webview = pair.second;
        return webview->surface.get() == surface;
      });
  if (iter == webviews_.end()) {
    focused_webview_ = nullptr;
    return;
  }

  const Webview* webview = iter->second.get();
  focused_webview_ = webview;
  SendTouchInput(focused_webview_, touch_point.x(), touch_point.y(),
                 ui::ET_TOUCH_PRESSED, time, id);
  points_[id] = touch_point;
}

void WebviewClient::HandleMode(void* data,
                               struct wl_output* wl_output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh) {
  if ((WL_OUTPUT_MODE_CURRENT & flags) != WL_OUTPUT_MODE_CURRENT)
    return;

  size_.SetSize(width, height);
  switch (transform_) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
      surface_size_.SetSize(width, height);
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
      surface_size_.SetSize(height, width);
      break;
    default:
      NOTREACHED();
      break;
  }

  std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
      wl_compositor_create_region(globals_.compositor.get())));

  if (!opaque_region) {
    LOG(ERROR) << "Can't create region";
    return;
  }

  wl_region_add(opaque_region.get(), 0, 0, surface_size_.width(),
                surface_size_.height());
  wl_surface_set_opaque_region(surface_.get(), opaque_region.get());
  wl_surface_set_input_region(surface_.get(), opaque_region.get());
}

void WebviewClient::HandleMotion(void* data,
                                 struct wl_touch* wl_touch,
                                 uint32_t time,
                                 int32_t id,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  if (!focused_webview_)
    return;
  gfx::Point& touch_point = points_[id];
  touch_point.set_x(wl_fixed_to_int(x));
  touch_point.set_y(wl_fixed_to_int(y));
  SendTouchInput(focused_webview_, touch_point.x(), touch_point.y(),
                 ui::ET_TOUCH_MOVED, time, id);
}

void WebviewClient::HandleUp(void* data,
                             struct wl_touch* wl_touch,
                             uint32_t serial,
                             uint32_t time,
                             int32_t id) {
  if (!focused_webview_)
    return;
  const gfx::Point& touch_point = points_[id];
  SendTouchInput(focused_webview_, touch_point.x(), touch_point.y(),
                 ui::ET_TOUCH_RELEASED, time, id);
  points_.erase(id);
}

void WebviewClient::InputCallback() {
  std::string request;
  getline(std::cin, request);

  if (request == "q") {
    run_loop_.Quit();
    return;
  }

  std::vector<std::string> tokens = SplitString(
      request, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.size() == 0)
    return;

  if (tokens[0] == kCreateCommand)
    CreateWebview(tokens);
  else if (tokens[0] == kDestroyCommand)
    DestroyWebview(tokens);
  else if (tokens[0] == kListCommand)
    ListActiveWebviews();
  else if (tokens[1] == kNavigateCommand)
    SendNavigationRequest(tokens);
  else if (tokens[1] == kResizeCommand)
    SendResizeRequest(tokens);
  else if (tokens[1] == kPositionCommand)
    SetPosition(tokens);
  else if (tokens[1] == kBackCommand)
    SendBackRequest(tokens);
  else if (tokens[1] == kForwardCommand)
    SendForwardRequest(tokens);
  else if (tokens[1] == kKeyCommand)
    SendKeyRequest(tokens);

  std::cout << "Enter command: ";
  std::cout.flush();
}

void WebviewClient::ListActiveWebviews() {
  for (const auto& pair : webviews_)
    std::cout << pair.first << std::endl;
}

void WebviewClient::Paint() {
  Buffer* buffer = DequeueBuffer();

  if (!buffer)
    return;

  if (gr_context_) {
    gr_context_->flushAndSubmit();
    glFinish();
  }

  wl_surface_set_buffer_scale(surface_.get(), scale_);
  wl_surface_set_buffer_transform(surface_.get(), transform_);
  wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                    surface_size_.height());
  wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

  // Set up frame callbacks.
  frame_callback_.reset(wl_surface_frame(surface_.get()));
  static wl_callback_listener frame_listener = {FrameCallback};
  wl_callback_add_listener(frame_callback_.get(), &frame_listener, this);

  for (const auto& pair : webviews_) {
    Webview* webview = pair.second.get();
    wl_surface_set_buffer_scale(webview->surface.get(), scale_);
    wl_surface_damage(webview->surface.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(webview->surface.get(), webview->buffer->buffer.get(), 0,
                      0);
    wl_surface_commit(webview->surface.get());
  }

  wl_surface_commit(surface_.get());
  wl_display_flush(display_.get());
}

void WebviewClient::SchedulePaint() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebviewClient::Paint, base::Unretained(this)));
}

void WebviewClient::SendBackRequest(const std::vector<std::string>& tokens) {
  int id;
  if (tokens.size() != 2 || !base::StringToInt(tokens[0], &id) ||
      webviews_.find(id) == webviews_.end()) {
    LOG(ERROR) << "Usage: [ID] back";
    return;
  }

  const auto& webview = webviews_[id];
  WebviewRequest back_request;
  back_request.mutable_go_back();
  if (!webview->client->Write(back_request)) {
    LOG(ERROR) << ("Back request send failed");
  }
}

void WebviewClient::SendForwardRequest(const std::vector<std::string>& tokens) {
  int id;
  if (tokens.size() != 2 || !base::StringToInt(tokens[0], &id) ||
      webviews_.find(id) == webviews_.end()) {
    LOG(ERROR) << "Usage: [ID] forward";
    return;
  }

  const auto& webview = webviews_[id];
  WebviewRequest forward_request;
  forward_request.mutable_go_forward();
  if (!webview->client->Write(forward_request)) {
    LOG(ERROR) << ("Forward request send failed");
  }
}

void WebviewClient::SendNavigationRequest(
    const std::vector<std::string>& tokens) {
  int id;
  if (tokens.size() != 3 || !base::StringToInt(tokens[0], &id) ||
      webviews_.find(id) == webviews_.end()) {
    LOG(ERROR) << "Usage: [ID] navigate [URL]";
    return;
  }

  const auto& webview = webviews_[id];
  WebviewRequest load_url_request;
  load_url_request.mutable_navigate()->set_url(tokens[2]);
  if (!webview->client->Write(load_url_request)) {
    LOG(ERROR) << ("Navigation request send failed");
  }
}

void WebviewClient::SendResizeRequest(const std::vector<std::string>& tokens) {
  int id, width, height;
  if (tokens.size() != 4 || !base::StringToInt(tokens[0], &id) ||
      webviews_.find(id) == webviews_.end() ||
      !base::StringToInt(tokens[2], &width) ||
      !base::StringToInt(tokens[3], &height)) {
    LOG(ERROR) << "Usage: [ID] resize [WIDTH] [HEIGHT]";
    return;
  }

  const auto& webview = webviews_[id];
  WebviewRequest resize_request;
  resize_request.mutable_resize()->set_width(width);
  resize_request.mutable_resize()->set_height(height);
  if (!webview->client->Write(resize_request)) {
    LOG(ERROR) << ("Resize request failed");
    return;
  }
  webview->buffer =
      CreateBuffer(gfx::Size(width, height), drm_format_, bo_usage_);
}

void WebviewClient::SendKeyRequest(const std::vector<std::string>& tokens) {
  int id;
  if (tokens.size() != 3 || !base::StringToInt(tokens[0], &id) ||
      tokens[2].empty()) {
    LOG(ERROR) << "Usage: ID key [dom_code]";
    return;
  }

  const auto& webview = webviews_[id];

  ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(tokens[2]);
  if (dom_code == ui::DomCode::NONE) {
    LOG(ERROR) << "Unknown DomCode CodeString: " << tokens[2];
    return;
  }

  ui::DomKey dom_key;
  ui::KeyboardCode keyboard_code;
  if (!ui::DomCodeToUsLayoutDomKey(dom_code, 0 /* flags */, &dom_key,
                                   &keyboard_code)) {
    LOG(ERROR) << "Could not convert DomCode to US Layout key: " << tokens[2];
    return;
  }

  SendKeyEvent(webview.get(), base::Time::Now().ToDeltaSinceWindowsEpoch(),
               dom_key, dom_code, keyboard_code, true);
  SendKeyEvent(webview.get(), base::Time::Now().ToDeltaSinceWindowsEpoch(),
               dom_key, dom_code, keyboard_code, false);
}

void WebviewClient::SendKeyEvent(const Webview* webview,
                                 const base::TimeDelta& time,
                                 ui::DomKey dom_key,
                                 ui::DomCode dom_code,
                                 ui::KeyboardCode keyboard_code,
                                 bool down) {
  auto key_input = std::make_unique<KeyInput>();
  key_input->set_key_code(keyboard_code);
  key_input->set_dom_code(static_cast<int32_t>(dom_code));
  key_input->set_dom_key(static_cast<int32_t>(dom_key));

  // Hardcoded to true for our purposes, as IsCharacter doesn't seem to work
  // here. Just means we can't test with modifier keys.
  key_input->set_is_char(true);

  auto key_event = std::make_unique<InputEvent>();
  key_event->set_event_type(down ? ui::EventType::ET_KEY_PRESSED
                                 : ui::EventType::ET_KEY_RELEASED);
  key_event->set_timestamp(time.InMicroseconds());
  key_event->set_allocated_key(key_input.release());

  WebviewRequest key_request;
  key_request.set_allocated_input(key_event.release());
  if (!webview->client->Write(key_request))
    LOG(ERROR) << "Key request failed";
}

void WebviewClient::SendTouchInput(const Webview* webview,
                                   int x,
                                   int y,
                                   ui::EventType event_type,
                                   uint32_t time,
                                   int32_t id) {
  auto touch_input = std::make_unique<TouchInput>();
  touch_input->set_x(x);
  touch_input->set_y(y);
  touch_input->set_root_x(x);
  touch_input->set_root_y(y);
  touch_input->set_pointer_type(static_cast<int>(ui::EventPointerType::kTouch));
  touch_input->set_pointer_id(id);

  auto input_event = std::make_unique<InputEvent>();
  input_event->set_event_type(event_type);
  input_event->set_timestamp(time * base::Time::kMicrosecondsPerMillisecond);
  input_event->set_allocated_touch(touch_input.release());

  WebviewRequest input_request;
  input_request.set_allocated_input(input_event.release());
  if (!webview->client->Write(input_request))
    LOG(ERROR) << "Input request failed";
}

void WebviewClient::SetPosition(const std::vector<std::string>& tokens) {
  int id, x, y;
  if (tokens.size() != 4 || !base::StringToInt(tokens[0], &id) ||
      webviews_.find(id) == webviews_.end() ||
      !base::StringToInt(tokens[2], &x) || !base::StringToInt(tokens[3], &y)) {
    LOG(ERROR) << "Usage: [ID] position [X] [Y]";
    return;
  }

  const auto& webview = webviews_[id];
  wl_subsurface_set_position(webview->subsurface.get(), x, y);
}

void WebviewClient::TakeExclusiveAccess() {
  while (wl_display_prepare_read(display_.get()) == -1) {
    if (wl_display_dispatch_pending(display_.get()) == -1) {
      LOG(ERROR) << "Error dispatching Wayland events";
      return;
    }
  }
  wl_display_flush(display_.get());
}

void WebviewClient::WlDisplayCallback() {
  wl_display_read_events(display_.get());
  TakeExclusiveAccess();
}

}  // namespace client
}  // namespace chromecast
