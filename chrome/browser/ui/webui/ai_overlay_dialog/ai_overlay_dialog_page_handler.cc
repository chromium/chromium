// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_untrusted_ui.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/generated_tool_definitions.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"
#include "chrome/common/chrome_switches.h"
#include "components/optimization_guide/content/browser/page_content_image_extractor.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#endif

namespace {

content::WebContents* GetActiveWebContentsFromBrowser(
    BrowserWindowInterface* browser) {
#if !BUILDFLAG(IS_ANDROID)
  return browser->GetTabStripModel()->GetActiveWebContents();
#else
  return nullptr;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
class AnimatedIconSource : public gfx::CanvasImageSource {
 public:
  static constexpr int kIconSize = ui::SimpleMenuModel::kDefaultIconSize;  // 16
  static constexpr int kCanvasSize = 22;

  AnimatedIconSource(const gfx::VectorIcon* icon, float energy, SkColor color)
      : CanvasImageSource(gfx::Size(kCanvasSize, kCanvasSize)),
        icon_(icon),
        energy_(energy),
        color_(color) {}

  void Draw(gfx::Canvas* canvas) override {
    // 1. Draw the vector icon in the center of our expanded canvas
    int offset = (kCanvasSize - kIconSize) / 2;
    canvas->Save();
    canvas->Translate(gfx::Vector2d(offset, offset));
    gfx::PaintVectorIcon(canvas, *icon_, kIconSize, color_);
    canvas->Restore();

    // 2. Draw the animated circle if energy is present
    if (energy_ > 0.01f) {
      // Scale from 1.0 to 1.4 of the *icon's* original bounds
      float radius_fraction = 1.0f + (0.4f * energy_);
      float base_radius = kIconSize / 2.0f;
      float radius = base_radius * radius_fraction;

      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1.5f);
      flags.setColor(SK_ColorWHITE);

      // Draw relative to the expanded canvas center
      gfx::PointF center(kCanvasSize / 2.0f, kCanvasSize / 2.0f);
      canvas->DrawCircle(center, radius, flags);
    }
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  float energy_;
  SkColor color_;
};
#endif

void SaveDebugFileAsync(base::FilePath dir_path,
                        base::FilePath file_path,
                        std::string content,
                        bool is_image) {
  std::string data_to_write = std::move(content);
  const std::string base64_prefix = "data:image/jpeg;base64,";
  if (base::StartsWith(data_to_write, base64_prefix)) {
    std::string decoded;
    if (base::Base64Decode(data_to_write.substr(base64_prefix.size()),
                           &decoded)) {
      data_to_write = std::move(decoded);
    }
  } else if (is_image) {
    std::string decoded;
    if (base::Base64Decode(data_to_write, &decoded)) {
      data_to_write = std::move(decoded);
    }
  }

  base::CreateDirectory(dir_path);
  base::WriteFile(file_path, data_to_write);
}

}  // namespace

namespace ttc {

AiOverlayDialogPageHandler::AiOverlayDialogPageHandler(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
    BrowserWindowInterface* browser,
    AiOverlayDialogUntrustedUI* untrusted_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(remote)),
      browser_(browser),
      untrusted_ui_(untrusted_ui) {
  if (auto* controller = AiOverlayDialogController::From(browser_)) {
    controller->AddObserver(this);
    page_->SetInputCaptionsVisible(controller->input_captions_visible());
    page_->SetOutputCaptionsVisible(controller->output_captions_visible());
    page_->SetUsePersona(controller->use_persona());
  }
}

AiOverlayDialogPageHandler::~AiOverlayDialogPageHandler() {
  if (auto* controller = AiOverlayDialogController::From(browser_)) {
    controller->RemoveObserver(this);
  }
}

void AiOverlayDialogPageHandler::GetMockAudioData(
    GetMockAudioDataCallback callback) {
  std::string path_string = features::kAiOverlayDialogMockJsonPath.Get();
  std::replace(path_string.begin(), path_string.end(), '+', '/');
  if (path_string.empty()) {
    VLOG(1) << "MockAudioData path not specified";
    std::move(callback).Run(std::nullopt);
    return;
  }

  VLOG(1) << "Using MockAudioData from: " << path_string;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const std::string& path_string) -> std::optional<std::string> {
            std::string data;
            if (!base::ReadFileToString(
                    base::FilePath::FromUTF8Unsafe(path_string), &data)) {
              return std::nullopt;
            }

            VLOG(1) << "\tMockAudioData head: " << data.substr(0, 100);
            return data;
          },
          path_string),
      std::move(callback));
}

void AiOverlayDialogPageHandler::UpdateAudioEnergy(float energy) {
#if !BUILDFLAG(IS_ANDROID)
  if (!overlay_action_item_) {
    overlay_action_item_ = actions::ActionManager::Get().FindAction(
        kActionShowAiOverlayDialog,
        BrowserActions::From(browser_)->root_action_item());
  }

  if (overlay_action_item_) {
    auto* controller = AiOverlayDialogController::From(browser_);
    const gfx::VectorIcon* base_icon =
        (controller && controller->IsOverlayShowing())
            ? &(features::IsRoundedIconsEnabled()
                    ? vector_icons::kPauseFilledIcon
                    : vector_icons::kPauseOldIcon)
            : &(features::IsRoundedIconsEnabled() ? vector_icons::kMicFilledIcon
                                                  : vector_icons::kMicOldIcon);

    overlay_action_item_->SetImage(ui::ImageModel::FromImageGenerator(
        base::BindRepeating(
            [](const gfx::VectorIcon* icon, float current_energy,
               const ui::ColorProvider* color_provider) -> gfx::ImageSkia {
              SkColor icon_color = color_provider->GetColor(ui::kColorIcon);
              return gfx::CanvasImageSource::MakeImageSkia<AnimatedIconSource>(
                  icon, current_energy, icon_color);
            },
            base_icon, energy),
        gfx::Size(AnimatedIconSource::kCanvasSize,
                  AnimatedIconSource::kCanvasSize)));
  }
#endif
}

void AiOverlayDialogPageHandler::Close() {
  if (auto* controller = AiOverlayDialogController::From(browser_)) {
    // HideOverlay() turns off listening and closes the overlay WebUI dialog interface.
    // TODO(crbug.com/540858790): Rename HideOverlay() to CloseOverlay() for clarity.
    controller->HideOverlay();
  }
}

void AiOverlayDialogPageHandler::DidChangePage(
    const GURL& url,
    const std::optional<std::u16string>& title,
    const std::optional<std::string>& content) {
  VLOG(1) << "Did Change Page";
  VLOG(1) << "\tURL: " << url.spec();
  if (title.has_value()) {
    VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title.value());
  }
  if (content.has_value()) {
    VLOG(1) << "\tContent: " << content.value().substr(0, 200) << "...";
  }

  page_->DidChangePage(
      url.spec(),
      title.has_value() ? std::make_optional(base::UTF16ToUTF8(title.value()))
                        : std::nullopt,
      content);

  if (ttc_mes_client_ && ttc_mes_client_->is_connected()) {
    PageContextMonitor* pcm =
        untrusted_ui_ ? untrusted_ui_->page_context_monitor() : nullptr;
    if (pcm && pcm->last_page_content().has_value()) {
      ttc_mes_client_->SendContextUpdate(
          url, title.has_value() ? base::UTF16ToUTF8(title.value()) : "",
          *pcm->last_page_content());
    }
  }
}

void AiOverlayDialogPageHandler::UpdateCurrentPageContext(
    const std::u16string& title,
    ai_overlay_dialog::mojom::PageContentNodePtr root_node) {
  VLOG(1) << "Update Current Page Context";
  VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title);

  page_->UpdateCurrentPageContext(base::UTF16ToUTF8(title),
                                  std::move(root_node));
}

void AiOverlayDialogPageHandler::OnInputCaptionsVisibleChanged(bool visible) {
  page_->SetInputCaptionsVisible(visible);
}

void AiOverlayDialogPageHandler::OnOutputCaptionsVisibleChanged(bool visible) {
  page_->SetOutputCaptionsVisible(visible);
}

void AiOverlayDialogPageHandler::OnUsePersonaChanged(bool use_persona) {
  page_->SetUsePersona(use_persona);
}

void AiOverlayDialogPageHandler::GetCursorPosition(
    GetCursorPositionCallback callback) {
  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  gfx::Point cursor_screen = screen->GetCursorScreenPoint();

  content::WebContents* web_contents =
      GetActiveWebContentsFromBrowser(browser_);

  if (!web_contents) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  gfx::Rect tab_bounds = web_contents->GetContainerBounds();
  if (!tab_bounds.Contains(cursor_screen)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  gfx::Point cursor_local = cursor_screen - tab_bounds.OffsetFromOrigin();
  std::move(callback).Run(cursor_local);
}

void AiOverlayDialogPageHandler::CaptureRawViewportRegion(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    CaptureRawViewportRegionCallback callback) {
  content::WebContents* web_contents =
      GetActiveWebContentsFromBrowser(browser_);

  if (!web_contents) {
    std::move(callback).Run(nullptr);
    return;
  }

  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (!view) {
    std::move(callback).Run(nullptr);
    return;
  }

  gfx::Rect crop_rect_logical =
      gfx::IntersectRects(gfx::Rect(web_contents->GetContainerBounds().size()),
                          gfx::Rect(x, y, width, height));

  float scale = view->GetDeviceScaleFactor();

  view->CopyFromSurface(
      crop_rect_logical, gfx::Size(), base::TimeDelta(),
      base::BindOnce(
          [](float scale, CaptureRawViewportRegionCallback cb,
             const content::CopyFromSurfaceResult& result) {
            if (!result.has_value() || result->bitmap.drawsNothing()) {
              std::move(cb).Run(nullptr);
              return;
            }

            const SkBitmap& bitmap = result->bitmap;
            std::optional<std::vector<uint8_t>> jpeg_bytes =
                gfx::JPEGCodec::Encode(bitmap.pixmap(), 85);
            if (!jpeg_bytes.has_value()) {
              std::move(cb).Run(nullptr);
              return;
            }

            std::string b64_data = base::Base64Encode(*jpeg_bytes);
            auto res = ai_overlay_dialog::mojom::RawViewportRegionResult::New();
            res->jpeg_data_b64 = b64_data;
            res->width = bitmap.width();
            res->height = bitmap.height();
            res->scale_factor = scale;
            std::move(cb).Run(std::move(res));
          },
          scale, std::move(callback)));
}

void AiOverlayDialogPageHandler::SetRememberedNote(
    ai_overlay_dialog::mojom::RememberedNotePtr note,
    SetRememberedNoteCallback callback) {
  if (!note || note->key.empty()) {
    std::move(callback).Run(false);
    return;
  }
  auto* controller = AiOverlayDialogController::From(browser_);
  if (!controller) {
    std::move(callback).Run(false);
    return;
  }
  controller->SetRememberedNote(note->key, note->value);
  std::move(callback).Run(true);
}

void AiOverlayDialogPageHandler::GetRememberedNotes(
    GetRememberedNotesCallback callback) {
  auto* controller = AiOverlayDialogController::From(browser_);
  if (!controller) {
    std::move(callback).Run({});
    return;
  }

  std::vector<ai_overlay_dialog::mojom::RememberedNotePtr> result;
  auto notes = controller->GetRememberedNotes();
  result.reserve(notes.size());
  for (const auto& [key, value] : notes) {
    auto note = ai_overlay_dialog::mojom::RememberedNote::New();
    note->key = key;
    note->value = value;
    result.push_back(std::move(note));
  }
  std::move(callback).Run(std::move(result));
}

void AiOverlayDialogPageHandler::SaveDebugFile(
    ai_overlay_dialog::mojom::DebugFileType type,
    const std::string& content) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableTtcDebugLogs)) {
    return;
  }

  base::FilePath filename;
  bool is_image = false;
  switch (type) {
    case ai_overlay_dialog::mojom::DebugFileType::kPrimingTurnMarkdown:
      filename = base::FilePath(FILE_PATH_LITERAL("priming_turn.md"));
      break;
    case ai_overlay_dialog::mojom::DebugFileType::kImage:
      filename = base::FilePath(FILE_PATH_LITERAL("image.jpg"));
      is_image = true;
      break;
  }

  base::FilePath dir_path(FILE_PATH_LITERAL("/tmp/ttc"));
  base::FilePath file_path = dir_path.Append(filename);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SaveDebugFileAsync, dir_path, file_path, content,
                     is_image));
}

void AiOverlayDialogPageHandler::GetImageBytes(
    const blink::DOMNodeIdType& dom_node_id,
    GetImageBytesCallback callback) {
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
  if (!contents || !contents->GetPrimaryMainFrame()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::optional<std::string> document_identifier =
      optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          contents->GetPrimaryMainFrame()->GetGlobalFrameToken());
  if (!document_identifier.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  PageContextMonitor* page_context_monitor =
      untrusted_ui_ ? untrusted_ui_->page_context_monitor() : nullptr;
  if (!page_context_monitor) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::optional<int32_t> resolved_id =
      page_context_monitor->ResolveImageDomNodeId(*document_identifier,
                                                  dom_node_id.value());
  if (!resolved_id.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  optimization_guide::GetImageBytes(
      contents, *document_identifier, *resolved_id,
      base::BindOnce(
          [](GetImageBytesCallback cb,
             blink::mojom::AIPageContentImageBytesResultPtr result) {
            if (!result || result->image_bytes.size() == 0 ||
                !result->image_info ||
                !result->image_info->mime_type.has_value() ||
                result->image_info->mime_type->empty()) {
              std::move(cb).Run(nullptr);
              return;
            }
            auto out_result = ai_overlay_dialog::mojom::ImageBytesResult::New();
            out_result->image_bytes = std::move(result->image_bytes);
            out_result->mime_type = *result->image_info->mime_type;
            std::move(cb).Run(std::move(out_result));
          },
          std::move(callback)));
}

void AiOverlayDialogPageHandler::StartStreamingSession() {
  if (!features::kAiOverlayDialogUseMes.Get()) {
    VLOG(1) << "StartStreamingSession called but use_mes is disabled";
    return;
  }
  if (!ttc_mes_client_) {
    ttc_mes_client_ =
        std::make_unique<TtcMesClient>(browser_->GetProfile(), this);
  }
  ttc_mes_client_->Connect();
  SendToolSetUpdate();
}

void AiOverlayDialogPageHandler::SendToolSetUpdate() {
  if (!ttc_mes_client_) {
    return;
  }

  std::vector<ToolDefinition> tools;
  std::optional<base::ListValue> root = base::JSONReader::ReadList(
      kBuiltInToolDefinitionsJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (root && !root->empty()) {
    const base::DictValue* container = (*root)[0].GetIfDict();
    const base::ListValue* decls =
        container ? container->FindList("functionDeclarations") : nullptr;
    if (decls) {
      for (const auto& item : *decls) {
        const base::DictValue* decl = item.GetIfDict();
        if (!decl) {
          continue;
        }
        const std::string* name = decl->FindString("name");
        const std::string* desc = decl->FindString("description");
        if (!name || !desc) {
          continue;
        }

        ToolDefinition tool;
        tool.name = *name;
        tool.description = *desc;

        const base::DictValue* params = decl->FindDict("parameters");
        if (params) {
          tool.parameters_json_schema = params->Clone();
        }

        const std::string* behavior = decl->FindString("behavior");
        if (behavior && *behavior == "NON_BLOCKING") {
          tool.behavior = ToolDefinition::Behavior::kNonBlocking;
        } else {
          tool.behavior = ToolDefinition::Behavior::kBlocking;
        }

        if (*name == "open_url" || *name == "switch_tab" ||
            *name == "close_current_tab" || *name == "go_back" ||
            *name == "go_forward" || *name == "reload_page" ||
            *name == "scroll" || *name == "play_video" ||
            *name == "pause_video" || *name == "click_element" ||
            *name == "set_text" || *name == "select_option") {
          tool.verbalization = ToolDefinition::Verbalization::kSilentAction;
        } else {
          tool.verbalization = ToolDefinition::Verbalization::kStandard;
        }

        tools.push_back(std::move(tool));
      }
    }
  }

  // Add extra built-in action tools.
  {
    ToolDefinition close_tool;
    close_tool.name = "close_voice_interface";
    close_tool.description =
        "Close voice interface and stop listening/speaking.";
    close_tool.behavior = ToolDefinition::Behavior::kNonBlocking;
    close_tool.verbalization = ToolDefinition::Verbalization::kSilentAction;
    tools.push_back(std::move(close_tool));
  }
  {
    ToolDefinition rem_tool;
    rem_tool.name = "remember_this";
    rem_tool.description =
        "Save a conversational fact or note to remember for later. "
        "Use this tool when the user asks you to remember something for "
        "later.";
    std::optional<base::DictValue> rem_params = base::JSONReader::ReadDict(
        R"({"type":"OBJECT","properties":{"key":{"type":"STRING"},)"
        R"("value":{"type":"STRING"}},"required":["key","value"]})",
        base::JSON_PARSE_RFC);
    if (rem_params) {
      rem_tool.parameters_json_schema = std::move(*rem_params);
    }
    tools.push_back(std::move(rem_tool));
  }
  {
    ToolDefinition forget_tool;
    forget_tool.name = "forget_this";
    forget_tool.description =
        "Delete a remembered conversational fact or note.";
    std::optional<base::DictValue> forget_params = base::JSONReader::ReadDict(
        R"({"type":"OBJECT","properties":{"key":{"type":"STRING"}},)"
        R"("required":["key"]})",
        base::JSON_PARSE_RFC);
    if (forget_params) {
      forget_tool.parameters_json_schema = std::move(*forget_params);
    }
    tools.push_back(std::move(forget_tool));
  }

  VLOG(1) << "Sending ToolSetUpdate with " << tools.size() << " active tools";
  ttc_mes_client_->SendToolSetUpdate(tools);
}

void AiOverlayDialogPageHandler::SendAudioChunk(mojo_base::BigBuffer pcm_data) {
  if (ttc_mes_client_ && ttc_mes_client_->is_connected()) {
    auto span = base::span(pcm_data);
    std::vector<uint8_t> data(span.begin(), span.end());
    ttc_mes_client_->SendAudioChunk(data);
  }
}

void AiOverlayDialogPageHandler::SendTextInput(const std::string& text) {
  if (ttc_mes_client_ && ttc_mes_client_->is_connected()) {
    ttc_mes_client_->SendTextInput(text);
  }
}

void AiOverlayDialogPageHandler::ReportPlaybackStatus(
    int64_t last_played_sequence_number) {
  if (ttc_mes_client_ && ttc_mes_client_->is_connected()) {
    ttc_mes_client_->ReportPlaybackStatus(last_played_sequence_number);
  }
}

void AiOverlayDialogPageHandler::StopStreamingSession() {
  if (ttc_mes_client_) {
    ttc_mes_client_->Close();
  }
}

void AiOverlayDialogPageHandler::OnStreamingStateChanged(
    bool connected,
    const std::string& session_id,
    const std::string& error_message) {
  if (page_.is_bound()) {
    page_->OnStreamingSessionStateChanged(connected, session_id, error_message);
  }
}

void AiOverlayDialogPageHandler::OnTranscriptions(
    const std::string& input_transcription,
    const std::string& output_transcription) {
  if (page_.is_bound()) {
    page_->OnTranscriptions(input_transcription, output_transcription);
  }
}

void AiOverlayDialogPageHandler::OnAudioOutput(
    const std::vector<uint8_t>& audio_data,
    int64_t sequence_number) {
  if (page_.is_bound()) {
    mojo_base::BigBuffer buffer(audio_data);
    page_->OnAudioOutput(std::move(buffer), sequence_number);
  }
}

void AiOverlayDialogPageHandler::OnGenerationStateChanged(bool started,
                                                          bool completed,
                                                          bool interrupted) {
  if (page_.is_bound()) {
    page_->OnGenerationStateChanged(started, completed, interrupted);
  }
}

void AiOverlayDialogPageHandler::OnToolCall(
    const std::string& name,
    base::DictValue arguments,
    TtcMesClient::Observer::ToolResponseCallback response_callback) {
  VLOG(1) << "AiOverlayDialogPageHandler executing tool: name=" << name
          << ", args=" << arguments;

  auto send_error = [&response_callback](std::string error_message) {
    base::DictValue dict;
    dict.Set("error", std::move(error_message));
    std::move(response_callback).Run(std::move(dict));
  };

  auto send_status_ok = [&response_callback]() {
    base::DictValue dict;
    dict.Set("status", "ok");
    std::move(response_callback).Run(std::move(dict));
  };

  auto make_status_cb =
      [](TtcMesClient::Observer::ToolResponseCallback callback) {
        return base::BindOnce(
            [](TtcMesClient::Observer::ToolResponseCallback cb,
               base::expected<std::monostate, std::string> result) {
              base::DictValue dict;
              if (result.has_value()) {
                dict.Set("status", "ok");
              } else {
                dict.Set("error", result.error());
              }
              std::move(cb).Run(std::move(dict));
            },
            std::move(callback));
      };

  std::optional<int> dom_id;
  if (auto id = arguments.FindInt("dom_node_id")) {
    dom_id = *id;
  } else if (auto* val = arguments.Find("dom_node_id")) {
    if (val->is_double()) {
      dom_id = static_cast<int>(val->GetDouble());
    } else if (val->is_string()) {
      int parsed = 0;
      if (base::StringToInt(val->GetString(), &parsed)) {
        dom_id = parsed;
      }
    }
  }

  // Handle overlay-level and controller-level tools.
  if (name == "close_voice_interface") {
    AiOverlayDialogController* controller =
        AiOverlayDialogController::From(browser_);
    if (controller) {
      controller->HideOverlay();
    }
    send_status_ok();
    return;
  }

  if (name == "remember_this") {
    const std::string* key = arguments.FindString("key");
    const std::string* val = arguments.FindString("value");
    if (!key || key->empty() || !val) {
      send_error("Missing key or value");
      return;
    }
    AiOverlayDialogController* controller =
        AiOverlayDialogController::From(browser_);
    if (!controller) {
      send_error("Controller not available");
      return;
    }
    controller->SetRememberedNote(*key, *val);
    send_status_ok();
    return;
  }

  if (name == "forget_this") {
    const std::string* key = arguments.FindString("key");
    if (!key || key->empty()) {
      send_error("Missing key");
      return;
    }
    AiOverlayDialogController* controller =
        AiOverlayDialogController::From(browser_);
    if (!controller) {
      send_error("Controller not available");
      return;
    }
    controller->SetRememberedNote(*key, "");
    send_status_ok();
    return;
  }

  // Handle browser tools.
  AiOverlayTools* tools = untrusted_ui_ ? untrusted_ui_->tools() : nullptr;
  if (!tools) {
    send_error("AiOverlayTools unavailable");
    return;
  }

  if (name == "open_url") {
    const std::string* url = arguments.FindString("url");
    bool new_tab = arguments.FindBool("new_tab").value_or(false);
    if (!url) {
      send_error("Missing url parameter");
      return;
    }
    tools->OpenUrl(*url, new_tab, make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "follow_link") {
    const std::string* id = arguments.FindString("id");
    if (!id) {
      send_error("Missing id parameter");
      return;
    }
    tools->FollowLink(*id, make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "perform_search") {
    const std::string* query = arguments.FindString("query");
    bool new_tab = arguments.FindBool("new_tab").value_or(false);
    if (!query) {
      send_error("Missing query parameter");
      return;
    }
    tools->PerformSearch(*query, new_tab,
                         make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "switch_tab") {
    const std::string* query = arguments.FindString("query");
    if (!query) {
      send_error("Missing query parameter");
      return;
    }
    tools->SwitchTab(
        *query,
        base::BindOnce(
            [](TtcMesClient::Observer::ToolResponseCallback cb,
               base::expected<ai_overlay_dialog::mojom::SwitchTabResultPtr,
                              std::string> result) {
              base::DictValue dict;
              if (result.has_value() && result.value()) {
                dict.Set("status", "ok");
                dict.Set("title", result.value()->title);
                dict.Set("url", result.value()->url.spec());
                dict.Set("tab_id", result.value()->tab_id);
              } else {
                dict.Set("error",
                         result.has_value() ? "Null result" : result.error());
              }
              std::move(cb).Run(std::move(dict));
            },
            std::move(response_callback)));
    return;
  }

  if (name == "close_current_tab") {
    tools->CloseCurrentTab(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "go_back") {
    tools->GoBack(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "go_forward") {
    tools->GoForward(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "reload_page") {
    tools->ReloadPage(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "find_and_highlight") {
    const std::string* query = arguments.FindString("query");
    if (!query) {
      send_error("Missing query parameter");
      return;
    }
    tools->FindAndHighlight(*query,
                            make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "scroll") {
    const std::string* gran_str = arguments.FindString("granularity");
    double magnitude = arguments.FindDouble("magnitude").value_or(0.0);
    ai_overlay_dialog::mojom::ScrollGranularity granularity =
        (gran_str && *gran_str == "document")
            ? ai_overlay_dialog::mojom::ScrollGranularity::kDocument
            : ai_overlay_dialog::mojom::ScrollGranularity::kPage;
    tools->Scroll(granularity, magnitude,
                  make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "play_video") {
    tools->PlayVideo(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "pause_video") {
    tools->PauseVideo(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "seek_to_timestamp") {
    const std::string* timecode = arguments.FindString("timecode");
    if (!timecode) {
      send_error("Missing timecode parameter");
      return;
    }
    tools->SeekToTimestamp(*timecode,
                           make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "translate_page") {
    const std::string* lang = arguments.FindString("target_language");
    tools->TranslatePage(lang ? *lang : std::string(),
                         make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "add_bookmark") {
    tools->AddBookmark(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "remove_bookmark") {
    tools->RemoveBookmark(make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "open_page") {
    const std::string* query = arguments.FindString("query");
    if (!query) {
      send_error("Missing query parameter");
      return;
    }
    tools->OpenPage(*query,
                    base::BindOnce(
                        [](TtcMesClient::Observer::ToolResponseCallback cb,
                           base::expected<std::string, std::string> result) {
                          base::DictValue dict;
                          if (result.has_value()) {
                            std::optional<base::DictValue> parsed =
                                base::JSONReader::ReadDict(
                                    result.value(),
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
                            if (parsed) {
                              dict = std::move(*parsed);
                            } else {
                              dict.Set("result", result.value());
                            }
                          } else {
                            dict.Set("error", result.error());
                          }
                          std::move(cb).Run(std::move(dict));
                        },
                        std::move(response_callback)));
    return;
  }

  if (name == "set_text") {
    const std::string* text = arguments.FindString("text");
    if (!dom_id.has_value() || !text) {
      send_error("Missing dom_node_id or text parameter");
      return;
    }
    tools->SetText(blink::DOMNodeIdType(*dom_id), *text,
                   make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "click_element") {
    if (!dom_id.has_value()) {
      send_error("Missing dom_node_id parameter");
      return;
    }
    tools->ClickElement(blink::DOMNodeIdType(*dom_id),
                        make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "set_fullscreen") {
    bool fullscreen = arguments.FindBool("fullscreen").value_or(true);
    tools->SetFullscreen(fullscreen,
                         make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "select_option") {
    const std::string* value = arguments.FindString("value");
    if (!dom_id.has_value() || !value) {
      send_error("Missing dom_node_id or value parameter");
      return;
    }
    tools->SelectOption(blink::DOMNodeIdType(*dom_id), *value,
                        make_status_cb(std::move(response_callback)));
    return;
  }

  if (name == "open_gemini_panel" || name == "invoke_glic") {
    const std::string* prompt = arguments.FindString("prompt");
    if (!prompt) {
      send_error("Missing prompt parameter");
      return;
    }
    tools->OpenGeminiPanel(
        *prompt, base::BindOnce(
                     [](TtcMesClient::Observer::ToolResponseCallback cb,
                        base::expected<std::string, std::string> result) {
                       base::DictValue dict;
                       if (result.has_value()) {
                         dict.Set("status", "ok");
                         dict.Set("message", result.value());
                       } else {
                         dict.Set("error", result.error());
                       }
                       std::move(cb).Run(std::move(dict));
                     },
                     std::move(response_callback)));
    return;
  }

  send_error(base::StrCat({"Unknown tool: ", name}));
}

}  // namespace ttc
