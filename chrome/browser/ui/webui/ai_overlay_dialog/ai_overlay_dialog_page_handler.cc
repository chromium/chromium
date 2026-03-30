// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/generated_tool_definitions.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/events/event.h"

namespace {

using ExecuteToolCallback =
    ai_overlay_dialog::mojom::PageHandler::ExecuteToolCallback;

}  // namespace

AiOverlayDialogPageHandler::AiOverlayDialogPageHandler(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
    BrowserWindowInterface* browser)
    : receiver_(this, std::move(receiver)),
      page_(std::move(remote)),
      browser_(browser) {}

AiOverlayDialogPageHandler::~AiOverlayDialogPageHandler() = default;

AiOverlayDialogPageHandler::AnnotationTask::AnnotationTask(
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
    base::OnceCallback<void(AiOverlayToolResult)> callback)
    : receiver_(this, std::move(host_receiver)),
      agent_remote_(std::move(agent_remote)),
      callback_(std::move(callback)) {}

AiOverlayDialogPageHandler::AnnotationTask::~AnnotationTask() {
  if (callback_) {
    std::move(callback_).Run(base::unexpected("Task destroyed"));
  }
}

void AiOverlayDialogPageHandler::AnnotationTask::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {
  if (attachment_result == blink::mojom::AttachmentResult::kSuccess) {
    agent_remote_->ScrollIntoView(/*applies_focus=*/true);
    if (callback_) {
      base::DictValue result;
      result.Set("match_count", 1);
      std::move(callback_).Run(base::Value(std::move(result)));
    }
  } else {
    if (callback_) {
      std::move(callback_).Run(base::unexpected("No match found"));
    }
  }
}

void AiOverlayDialogPageHandler::OnAnnotationAgentDisconnected() {
  annotation_task_.reset();
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
}

void AiOverlayDialogPageHandler::UpdateCurrentPageContext(
    const std::u16string& title,
    const std::string& content) {
  VLOG(1) << "Update Current Page Context";
  VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title);
  VLOG(1) << "\tContent: " << content.substr(0, 200) << "...";

  page_->UpdateCurrentPageContext(base::UTF16ToUTF8(title), content);
}

// TODO(gklassen): Add tests for tool calling.

void AiOverlayDialogPageHandler::GetToolDefinitions(
    GetToolDefinitionsCallback callback) {
  std::move(callback).Run(
      std::string(ai_overlay_dialog::kBuiltInToolDefinitions));
}

void AiOverlayDialogPageHandler::ExecuteTool(const std::string& name,
                                             const std::string& json_args,
                                             ExecuteToolCallback callback) {
  if (!browser_) {
    std::move(callback).Run(
        "{\"success\": false, \"error\": \"No active browser\"}");
    return;
  }

  std::optional<base::Value> parsed_args =
      base::JSONReader::Read(json_args, base::JSON_PARSE_RFC);
  if (!parsed_args || !parsed_args->is_dict()) {
    std::move(callback).Run(
        "{\"success\": false, \"error\": \"Invalid JSON arguments\"}");
    return;
  }
  const base::DictValue& args = parsed_args->GetDict();

  auto result_callback = base::BindOnce(
      [](ExecuteToolCallback callback, AiOverlayToolResult result) {
        base::DictValue dict;
        if (result.has_value()) {
          dict.Set("success", true);
          // If the tool returned a dictionary, merge its fields into the result
          if (result.value().is_dict()) {
            dict.Merge(std::move(result.value().GetDict()));
          }
        } else {
          dict.Set("success", false);
          dict.Set("error", result.error());
        }
        std::string json;
        base::JSONWriter::Write(dict, &json);
        std::move(callback).Run(json);
      });

  if (name == "open_url") {
    const std::string* url = args.FindString("url");
    std::optional<bool> new_tab = args.FindBool("new_tab");
    if (!url || !new_tab.has_value()) {
      std::move(callback).Run(
          "{\"success\": false, \"error\": \"Missing arguments\"}");
      return;
    }
    OpenUrl(*url, *new_tab,
            base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "perform_search") {
    const std::string* query = args.FindString("query");
    std::optional<bool> new_tab = args.FindBool("new_tab");
    if (!query || !new_tab.has_value()) {
      std::move(callback).Run(
          "{\"success\": false, \"error\": \"Missing arguments\"}");
      return;
    }
    PerformSearch(
        *query, *new_tab,
        base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "switch_tab") {
    const std::string* query = args.FindString("query");
    if (!query) {
      std::move(callback).Run(
          "{\"success\": false, \"error\": \"Missing query\"}");
      return;
    }
    SwitchTab(*query,
              base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "close_current_tab") {
    CloseCurrentTab(
        base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "go_back") {
    GoBack(base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "go_forward") {
    GoForward(base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "reload_page") {
    ReloadPage(base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "find_and_highlight") {
    const std::string* query = args.FindString("query");
    if (!query) {
      std::move(callback).Run(
          "{\"success\": false, \"error\": \"Missing query\"}");
      return;
    }
    FindAndHighlight(*query, base::BindOnce(std::move(result_callback),
                                            std::move(callback)));
  } else if (name == "scroll") {
    const std::string* direction = args.FindString("direction");
    std::optional<double> magnitude = args.FindDouble("magnitude");
    if (!direction || !magnitude.has_value()) {
      std::move(callback).Run(
          "{\"success\": false, \"error\": \"Missing arguments\"}");
      return;
    }
    Scroll(*direction, *magnitude,
           base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "play_video") {
    PlayVideo(base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "pause_video") {
    PauseVideo(base::BindOnce(std::move(result_callback), std::move(callback)));
  } else if (name == "seek_to_timestamp") {
    const std::string* timecode = args.FindString("timecode");
    if (!timecode) {
      std::move(callback).Run(
          "{\"success\": false, \"error\": \"Missing timecode\"}");
      return;
    }
    SeekToTimestamp(*timecode, base::BindOnce(std::move(result_callback),
                                              std::move(callback)));
  } else {
    std::move(callback).Run(
        "{\"success\": false, \"error\": \"Unknown tool\"}");
  }
}

void AiOverlayDialogPageHandler::OpenUrl(
    const std::string& url_string,
    bool new_tab,
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  GURL url(url_string);
  if (!url.is_valid()) {
    std::move(callback).Run(base::unexpected("Invalid URL"));
    return;
  }

  WindowOpenDisposition disposition =
      new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
              : WindowOpenDisposition::CURRENT_TAB;
  browser_->OpenGURL(url, disposition);
  std::move(callback).Run(base::Value());
}

void AiOverlayDialogPageHandler::PerformSearch(
    const std::string& query,
    bool new_tab,
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser_->GetProfile());
  if (!template_url_service) {
    std::move(callback).Run(base::unexpected("Search service not available"));
    return;
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    std::move(callback).Run(
        base::unexpected("Default search provider not set"));
    return;
  }

  GURL url = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data(), base::UTF8ToUTF16(query));
  OpenUrl(url.spec(), new_tab, std::move(callback));
}

void AiOverlayDialogPageHandler::SwitchTab(
    const std::string& query,
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  std::string query_lower = base::ToLowerASCII(query);
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    std::string title =
        base::ToLowerASCII(base::UTF16ToUTF8(contents->GetTitle()));
    std::string url = base::ToLowerASCII(contents->GetURL().spec());
    if (title.find(query_lower) != std::string::npos ||
        url.find(query_lower) != std::string::npos) {
      tab_strip_model->ActivateTabAt(i);
      browser_->GetWindow()->Activate();

      base::DictValue matched_tab;
      matched_tab.Set("title", base::UTF16ToUTF8(contents->GetTitle()));
      matched_tab.Set("url", contents->GetURL().spec());
      matched_tab.Set("tab_id",
                      sessions::SessionTabHelper::IdForTab(contents).id());

      base::DictValue response;
      response.Set("matchedTab", std::move(matched_tab));
      std::move(callback).Run(base::Value(std::move(response)));
      return;
    }
  }
  std::move(callback).Run(base::unexpected("No matching tab found"));
}

void AiOverlayDialogPageHandler::CloseCurrentTab(
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  if (browser_->GetTabStripModel()->count() > 0) {
    browser_->GetTabStripModel()->CloseSelectedTabs();
    std::move(callback).Run(base::Value());
  } else {
    std::move(callback).Run(base::unexpected("No active tab to close"));
  }
}

void AiOverlayDialogPageHandler::GoBack(
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    std::move(callback).Run(base::Value());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go back"));
}

void AiOverlayDialogPageHandler::GoForward(
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (contents && contents->GetController().CanGoForward()) {
    contents->GetController().GoForward();
    std::move(callback).Run(base::Value());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go forward"));
}

void AiOverlayDialogPageHandler::ReloadPage(
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (contents) {
    contents->GetController().Reload(content::ReloadType::NORMAL, true);
    std::move(callback).Run(base::Value());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot reload page"));
}

void AiOverlayDialogPageHandler::FindAndHighlight(
    const std::string& query,
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (annotation_document_.AsRenderFrameHostIfValid() != rfh) {
    annotation_container_.reset();
    annotation_task_.reset();
    annotation_document_ = rfh->GetWeakDocumentPtr();
    rfh->GetRemoteInterfaces()->GetInterface(
        annotation_container_.BindNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_remote;

  auto agent_receiver = agent_remote.BindNewPipeAndPassReceiver();
  annotation_task_ = std::make_unique<AnnotationTask>(
      host_remote.InitWithNewPipeAndPassReceiver(), std::move(agent_remote),
      std::move(callback));

  // Use a text fragment selector for direct highlighting.
  // The format is "text=<text_to_highlight>"
  auto selector =
      blink::mojom::Selector::NewSerializedSelector("text=" + query);

  annotation_container_->CreateAgent(
      std::move(host_remote), std::move(agent_receiver),
      blink::mojom::AnnotationType::kGlic, std::move(selector),
      /*search_range_start_node_id=*/std::nullopt);
}

void AiOverlayDialogPageHandler::Scroll(
    const std::string& direction,
    double magnitude,
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents || !contents->GetRenderWidgetHostView()) {
    std::move(callback).Run(base::unexpected("No active tab or view"));
    return;
  }

  blink::WebMouseWheelEvent wheel_event;
  wheel_event.SetType(blink::WebInputEvent::Type::kMouseWheel);
  wheel_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;

  if (direction == "kTop" || direction == "kBottom") {
    wheel_event.delta_units = ui::ScrollGranularity::kScrollByDocument;
    double sign = (direction == "kTop") ? 1.0 : -1.0;
    wheel_event.delta_y = sign;
    wheel_event.wheel_ticks_y = sign;
  } else {
    double sign = (direction == "kUp") ? -1.0 : 1.0;
    float scale_factor =
        contents->GetRenderWidgetHostView()->GetDeviceScaleFactor();
    int scroll_amount = static_cast<int>(
        sign * magnitude * contents->GetContainerBounds().height() *
        scale_factor);

    wheel_event.delta_y = -scroll_amount;
    wheel_event.wheel_ticks_y =
        static_cast<float>(-scroll_amount) / ui::MouseWheelEvent::kWheelDelta;
  }

  content::RenderWidgetHost* widget_host =
      contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  widget_host->ForwardWheelEvent(wheel_event);

  // Send a synthetic wheel event with phaseEnded to finish scrolling.
  wheel_event.delta_y = 0;
  wheel_event.wheel_ticks_y = 0;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  wheel_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  widget_host->ForwardWheelEvent(wheel_event);

  std::move(callback).Run(base::Value());
}

void AiOverlayDialogPageHandler::PlayVideo(
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(contents);
  if (media_session) {
    media_session->Resume(content::MediaSession::SuspendType::kUI);
    std::move(callback).Run(base::Value());
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayDialogPageHandler::PauseVideo(
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(contents);
  if (media_session) {
    media_session->Suspend(content::MediaSession::SuspendType::kUI);
    std::move(callback).Run(base::Value());
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

namespace {

std::optional<base::TimeDelta> ParseTimecode(const std::string& timecode) {
  std::vector<std::string> parts = base::SplitString(
      timecode, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 1) {
    int s;
    if (base::StringToInt(parts[0], &s)) {
      return base::Seconds(s);
    }
  } else if (parts.size() == 2) {
    int m, s;
    if (base::StringToInt(parts[0], &m) && base::StringToInt(parts[1], &s)) {
      return base::Seconds(m * 60 + s);
    }
  } else if (parts.size() == 3) {
    int h, m, s;
    if (base::StringToInt(parts[0], &h) && base::StringToInt(parts[1], &m) &&
        base::StringToInt(parts[2], &s)) {
      return base::Seconds(h * 3600 + m * 60 + s);
    }
  }
  return std::nullopt;
}

}  // namespace

void AiOverlayDialogPageHandler::SeekToTimestamp(
    const std::string& timecode,
    base::OnceCallback<void(AiOverlayToolResult)> callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(contents);
  if (media_session) {
    std::optional<base::TimeDelta> seek_time = ParseTimecode(timecode);
    if (seek_time.has_value() &&
        (seek_time.value().is_positive() || seek_time.value().is_zero())) {
      media_session->SeekTo(seek_time.value());
      std::move(callback).Run(base::Value());
    } else {
      std::move(callback).Run(base::unexpected("Invalid timecode"));
    }
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}
