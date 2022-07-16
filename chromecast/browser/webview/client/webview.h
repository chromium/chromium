// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_CLIENT_WEBVIEW_H_
#define CHROMECAST_BROWSER_WEBVIEW_CLIENT_WEBVIEW_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chromecast/browser/webview/proto/webview.grpc.pb.h"
#include "components/exo/wayland/clients/client_base.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace chromecast {
namespace client {

// Sample Wayland client to manipulate webviews
class WebviewClient : public exo::wayland::clients::ClientBase {
 public:
  struct BufferCallback {
    WebviewClient* client;
    Buffer* buffer;
  };

  WebviewClient();

  WebviewClient(const WebviewClient&) = delete;
  WebviewClient& operator=(const WebviewClient&) = delete;

  ~WebviewClient() override;
  bool HasAvailableBuffer();
  void Run(const InitParams& params, const std::string& channel_directory);
  void SchedulePaint();

 private:
  using WebviewRequestResponseClient =
      ::grpc::ClientReaderWriterInterface<chromecast::webview::WebviewRequest,
                                          chromecast::webview::WebviewResponse>;
  struct Surface {
    Surface();
    virtual ~Surface();
    bool isWebview = false;
    std::unique_ptr<ClientBase::Buffer> buffer;
    std::unique_ptr<wl_surface> surface;
    std::unique_ptr<wl_subsurface> subsurface;
  };

  struct Webview : public Surface {
    Webview();
    ~Webview() override;
    static Webview* FromSurface(Surface* surface);
    std::unique_ptr<WebviewRequestResponseClient> client;
    std::unique_ptr<::grpc::ClientContext> context;
  };

  void AllocateBuffers(const InitParams& params);
  void CreateWebview(const std::vector<std::string>& tokens);
  void CreateSurface(const std::vector<std::string>& tokens);
  bool SetupSurface(const std::vector<std::string>& tokens,
                    Surface* surface,
                    int* id);
  void DestroySurface(const std::vector<std::string>& tokens);
  void HandleDown(void* data,
                  struct wl_touch* wl_touch,
                  uint32_t serial,
                  uint32_t time,
                  struct wl_surface* surface,
                  int32_t id,
                  wl_fixed_t x,
                  wl_fixed_t y) override;
  void HandleMode(void* data,
                  struct wl_output* wl_output,
                  uint32_t flags,
                  int32_t width,
                  int32_t height,
                  int32_t refresh) override;
  void HandleMotion(void* data,
                    struct wl_touch* wl_touch,
                    uint32_t time,
                    int32_t id,
                    wl_fixed_t x,
                    wl_fixed_t y) override;
  void HandleUp(void* data,
                struct wl_touch* wl_touch,
                uint32_t serial,
                uint32_t time,
                int32_t id) override;
  void InputCallback();
  void ListActiveSurfaces();
  void Paint();
  void SendBackRequest(const std::vector<std::string>& tokens);
  void SendForwardRequest(const std::vector<std::string>& tokens);
  void SendNavigationRequest(const std::vector<std::string>& tokens);
  void SendResizeRequest(Webview* webview, int width, int height);
  void HandleResizeRequest(const std::vector<std::string>& tokens);
  void HandleFillSurfaceColor(const std::vector<std::string>& tokens);
  void SendKeyRequest(const std::vector<std::string>& tokens);
  void HandleSetInsets(const std::vector<std::string>& tokens);
  void HandleFocus(const std::vector<std::string>& tokens);

  void SendTouchInput(const Webview* webview,
                      int x,
                      int y,
                      ui::EventType event_type,
                      uint32_t time,
                      int32_t id);
  void SendKeyEvent(const Webview* webview,
                    const base::TimeDelta& time,
                    const std::string& key_string,
                    bool down);

  void SetPosition(const std::vector<std::string>& tokens);
  void TakeExclusiveAccess();
  void WlDisplayCallback();

  int32_t drm_format_ = 0;
  int32_t bo_usage_ = 0;

  const Webview* focused_webview_;
  std::map<int, std::unique_ptr<Surface>> surfaces_;
  std::map<int32_t, gfx::Point> points_;

  std::unique_ptr<wl_callback> frame_callback_;
  std::vector<std::unique_ptr<BufferCallback>> buffer_callbacks_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<base::FileDescriptorWatcher::Controller> stdin_controller_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      wl_display_controller_;
  base::FileDescriptorWatcher file_descriptor_watcher_;
  base::RunLoop run_loop_;

  std::unique_ptr<chromecast::webview::PlatformViewsService::Stub> stub_;
};

}  // namespace client
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_CLIENT_WEBVIEW_H_
