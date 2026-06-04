// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/scroll_types.h"
#include "url/url_util.h"

namespace {

void RecordToolCallInvoked(std::string_view tool_name) {
  base::UmaHistogramBoolean(
      base::StrCat({"AI.OverlayDialog.ToolCallInvoked.", tool_name}), true);
}

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

namespace ttc {

AiOverlayTools::AiOverlayTools(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser)
    : receiver_(this, std::move(receiver)), browser_(browser) {}

AiOverlayTools::~AiOverlayTools() = default;

void AiOverlayTools::OpenUrl(const std::string& url_string,
                             bool new_tab,
                             OpenUrlCallback callback) {
  RecordToolCallInvoked("OpenUrl");
  GURL url(url_string);
  if (!url.is_valid()) {
    std::move(callback).Run(base::unexpected("Invalid URL"));
    return;
  }

  WindowOpenDisposition disposition =
      new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
              : WindowOpenDisposition::CURRENT_TAB;
  browser_->OpenGURL(url, disposition);
  std::move(callback).Run(std::monostate());
}

void AiOverlayTools::PerformSearch(const std::string& query,
                                   bool new_tab,
                                   PerformSearchCallback callback) {
  RecordToolCallInvoked("PerformSearch");
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser_->GetProfile());
  if (!template_url_service) {
    std::move(callback).Run(base::unexpected("Search service not available"));
    return;
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    std::move(callback).Run(base::unexpected("Default search provider not set"));
    return;
  }

  GURL url = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data(), base::UTF8ToUTF16(query));
  OpenUrl(url.spec(), new_tab, std::move(callback));
}

void AiOverlayTools::SwitchTab(const std::string& query,
                               SwitchTabCallback callback) {
  RecordToolCallInvoked("SwitchTab");
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

      auto result = ai_overlay_dialog::mojom::SwitchTabResult::New();
      result->title = base::UTF16ToUTF8(contents->GetTitle());
      result->url = contents->GetURL();
      result->tab_id = sessions::SessionTabHelper::IdForTab(contents).id();

      std::move(callback).Run(std::move(result));
      return;
    }
  }
  std::move(callback).Run(base::unexpected("No matching tab found"));
}

void AiOverlayTools::CloseCurrentTab(CloseCurrentTabCallback callback) {
  RecordToolCallInvoked("CloseCurrentTab");
  if (browser_->GetTabStripModel()->count() > 0) {
    browser_->GetTabStripModel()->CloseSelectedTabs();
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active tab to close"));
  }
}

void AiOverlayTools::GoBack(GoBackCallback callback) {
  RecordToolCallInvoked("GoBack");
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go back"));
}

void AiOverlayTools::GoForward(GoForwardCallback callback) {
  RecordToolCallInvoked("GoForward");
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (contents && contents->GetController().CanGoForward()) {
    contents->GetController().GoForward();
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go forward"));
}

void AiOverlayTools::ReloadPage(ReloadPageCallback callback) {
  RecordToolCallInvoked("ReloadPage");
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (contents) {
    contents->GetController().Reload(content::ReloadType::NORMAL, true);
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot reload page"));
}

AiOverlayTools::AnnotationTask::AnnotationTask(
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
    FindAndHighlightCallback callback)
    : receiver_(this, std::move(host_receiver)),
      agent_remote_(std::move(agent_remote)),
      callback_(std::move(callback)) {}

AiOverlayTools::AnnotationTask::~AnnotationTask() {
  if (callback_) {
    std::move(callback_).Run(base::unexpected("Task destroyed"));
  }
}

void AiOverlayTools::AnnotationTask::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {
  if (attachment_result == blink::mojom::AttachmentResult::kSuccess) {
    agent_remote_->ScrollIntoView(/*applies_focus=*/true);
    if (callback_) {
      std::move(callback_).Run(std::monostate());
    }
  } else {
    if (callback_) {
      std::move(callback_).Run(base::unexpected("No match found"));
    }
  }
}

void AiOverlayTools::OnAnnotationAgentDisconnected() {
  annotation_task_.reset();
}

void AiOverlayTools::FindAndHighlight(const std::string& query,
                                      FindAndHighlightCallback callback) {
  RecordToolCallInvoked("FindAndHighlight");
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
  auto selector = blink::mojom::Selector::NewSerializedSelector(
      url::EncodeUriComponent(query));

  annotation_container_->CreateAgent(
      std::move(host_remote), std::move(agent_receiver),
      blink::mojom::AnnotationType::kGlic, std::move(selector),
      /*search_range_start_node_id=*/std::nullopt);
}

void AiOverlayTools::Scroll(
    ai_overlay_dialog::mojom::ScrollGranularity granularity,
    double magnitude,
    ScrollCallback callback) {
  RecordToolCallInvoked("Scroll");
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents || !contents->GetRenderWidgetHostView()) {
    std::move(callback).Run(base::unexpected("No active tab or view"));
    return;
  }

  content::RenderWidgetHost* widget_host =
      contents->GetRenderWidgetHostView()->GetRenderWidgetHost();

  auto get_key_code =
      [](ai_overlay_dialog::mojom::ScrollGranularity granularity,
         double magnitude) {
        switch (granularity) {
          case ai_overlay_dialog::mojom::ScrollGranularity::kPage:
            return (magnitude > 0) ? ui::VKEY_NEXT : ui::VKEY_PRIOR;
          case ai_overlay_dialog::mojom::ScrollGranularity::kDocument:
            return (magnitude > 0) ? ui::VKEY_END : ui::VKEY_HOME;
        }
      };

  ui::KeyboardCode key_code = get_key_code(granularity, magnitude);

  // For Document granularity, we only need to send the key once to reach the
  // end or start. For Page granularity, we send it for each page requested.
  ui::KeyEvent pressed_event(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
  ui::KeyEvent released_event(ui::EventType::kKeyReleased, key_code,
                              ui::EF_NONE);
  widget_host->ForwardKeyboardEvent(
      input::NativeWebKeyboardEvent(pressed_event));
  widget_host->ForwardKeyboardEvent(
      input::NativeWebKeyboardEvent(released_event));

  std::move(callback).Run(std::monostate());
}

void AiOverlayTools::PlayVideo(PlayVideoCallback callback) {
  RecordToolCallInvoked("PlayVideo");
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
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayTools::PauseVideo(PauseVideoCallback callback) {
  RecordToolCallInvoked("PauseVideo");
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
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayTools::InvokeGlic(const std::string& prompt,
                                InvokeGlicCallback callback) {
  RecordToolCallInvoked("InvokeGlic");
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser_->GetProfile());

  if (!glic_service) {
    std::move(callback).Run(base::unexpected("Glic service not available"));
    return;
  }

  glic::GlicInvokeOptions options(
      glic::Target(browser_->GetTabStripModel()->GetActiveTab()),
      glic::mojom::InvocationSource::kOsButton);
  options.prompts.push_back(prompt);

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  options.on_success = base::BindOnce(
      [](InvokeGlicCallback cb) {
        std::move(cb).Run(base::ok("Glic panel opened and task completed."));
      },
      std::move(split_callback.first));

  options.on_error = base::BindOnce(
      [](InvokeGlicCallback cb, glic::GlicInvokeError error) {
        std::move(cb).Run(base::unexpected("Glic invocation failed"));
      },
      std::move(split_callback.second));

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options));
}

void AiOverlayTools::SeekToTimestamp(const std::string& timecode,
                                     SeekToTimestampCallback callback) {
  RecordToolCallInvoked("SeekToTimestamp");
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
      std::move(callback).Run(std::monostate());
    } else {
      std::move(callback).Run(base::unexpected("Invalid timecode"));
    }
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayTools::TranslatePage(const std::string& target_language,
                                   TranslatePageCallback callback) {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!translate_client) {
    std::move(callback).Run(base::unexpected("Translation not supported"));
    return;
  }

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  if (!translate_manager) {
    std::move(callback).Run(base::unexpected("Translation not available"));
    return;
  }

  if (target_language.empty()) {
    translate_manager->ShowTranslateUI(/*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  } else {
    if (!translate::TranslateDownloadManager::IsSupportedLanguage(
            target_language)) {
      std::move(callback).Run(base::unexpected("Unsupported language"));
      return;
    }

    translate_manager->ShowTranslateUI(std::nullopt, target_language,
                                       /*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  }

  std::move(callback).Run(std::monostate());
}

}  // namespace ttc
