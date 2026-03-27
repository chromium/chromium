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
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

AiOverlayDialogPageHandler::AiOverlayDialogPageHandler(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
    BrowserWindowInterface* browser)
    : receiver_(this, std::move(receiver)),
      page_(std::move(remote)),
      browser_(browser) {}

AiOverlayDialogPageHandler::~AiOverlayDialogPageHandler() = default;

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
  std::optional<base::Value> parsed_args =
      base::JSONReader::Read(json_args, base::JSON_PARSE_RFC);
  if (!parsed_args || !parsed_args->is_dict()) {
    std::move(callback).Run(
        "{\"success\": false, \"error\": \"Invalid JSON arguments\"}");
    return;
  }
  const base::DictValue& args = parsed_args->GetDict();

  auto bool_callback =
      base::BindOnce([](ExecuteToolCallback callback, bool success) {
        std::move(callback).Run(success ? "{\"success\": true}"
                                        : "{\"success\": false}");
      });

  auto dict_callback =
      base::BindOnce([](ExecuteToolCallback callback, base::DictValue dict) {
        std::string json;
        base::JSONWriter::Write(dict, &json);
        std::move(callback).Run(json);
      });

  if (name == "open_url") {
    const std::string* url = args.FindString("url");
    std::optional<bool> new_tab = args.FindBool("new_tab");
    if (!url || !new_tab.has_value()) {
      std::move(callback).Run("{\"success\": false}");
      return;
    }
    OpenUrl(*url, *new_tab,
            base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "perform_search") {
    const std::string* query = args.FindString("query");
    std::optional<bool> new_tab = args.FindBool("new_tab");
    if (!query || !new_tab.has_value()) {
      std::move(callback).Run("{\"success\": false}");
      return;
    }
    PerformSearch(
        *query, *new_tab,
        base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "switch_tab") {
    const std::string* query = args.FindString("query");
    if (!query) {
      std::move(callback).Run("{\"matchedTab\": null}");
      return;
    }
    SwitchTab(*query,
              base::BindOnce(std::move(dict_callback), std::move(callback)));
  } else if (name == "close_current_tab") {
    CloseCurrentTab(
        base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "go_back") {
    GoBack(base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "go_forward") {
    GoForward(base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "reload_page") {
    ReloadPage(base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "find_and_highlight") {
    const std::string* query = args.FindString("query");
    if (!query) {
      std::move(callback).Run("{\"error\": \"Missing query\"}");
      return;
    }
    FindAndHighlight(
        *query, base::BindOnce(std::move(dict_callback), std::move(callback)));
  } else if (name == "scroll") {
    const std::string* direction = args.FindString("direction");
    std::optional<double> magnitude = args.FindDouble("magnitude");
    if (!direction || !magnitude.has_value()) {
      std::move(callback).Run("{\"success\": false}");
      return;
    }
    Scroll(*direction, *magnitude,
           base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "play_video") {
    PlayVideo(base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "pause_video") {
    PauseVideo(base::BindOnce(std::move(bool_callback), std::move(callback)));
  } else if (name == "seek_to_timestamp") {
    const std::string* timecode = args.FindString("timecode");
    if (!timecode) {
      std::move(callback).Run("{\"success\": false}");
      return;
    }
    SeekToTimestamp(*timecode, base::BindOnce(std::move(bool_callback),
                                              std::move(callback)));
  } else {
    std::move(callback).Run(
        "{\"success\": false, \"error\": \"Unknown tool\"}");
  }
}

void AiOverlayDialogPageHandler::OpenUrl(
    const std::string& url_string,
    bool new_tab,
    base::OnceCallback<void(bool)> callback) {
  GURL url(url_string);
  if (!url.is_valid() || !browser_) {
    std::move(callback).Run(false);
    return;
  }

  WindowOpenDisposition disposition =
      new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
              : WindowOpenDisposition::CURRENT_TAB;
  browser_->OpenGURL(url, disposition);
  std::move(callback).Run(true);
}

void AiOverlayDialogPageHandler::PerformSearch(
    const std::string& query,
    bool new_tab,
    base::OnceCallback<void(bool)> callback) {
  if (!browser_) {
    std::move(callback).Run(false);
    return;
  }
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser_->GetProfile());
  if (!template_url_service) {
    std::move(callback).Run(false);
    return;
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void AiOverlayDialogPageHandler::SwitchTab(
    const std::string& query,
    base::OnceCallback<void(base::DictValue)> callback) {}

void AiOverlayDialogPageHandler::CloseCurrentTab(
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::GoBack(
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::GoForward(
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::ReloadPage(
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::FindAndHighlight(
    const std::string& query,
    base::OnceCallback<void(base::DictValue)> callback) {}

void AiOverlayDialogPageHandler::Scroll(
    const std::string& direction,
    double magnitude,
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::PlayVideo(
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::PauseVideo(
    base::OnceCallback<void(bool)> callback) {}

void AiOverlayDialogPageHandler::SeekToTimestamp(
    const std::string& timecode,
    base::OnceCallback<void(bool)> callback) {}

AiOverlayDialogPageHandler::AnnotationTask::AnnotationTask(
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
    base::OnceCallback<void(base::DictValue)> callback)
    : receiver_(this, std::move(host_receiver)),
      agent_remote_(std::move(agent_remote)),
      callback_(std::move(callback)) {}

AiOverlayDialogPageHandler::AnnotationTask::~AnnotationTask() = default;

void AiOverlayDialogPageHandler::AnnotationTask::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {}

void AiOverlayDialogPageHandler::AnnotationTask::ScrollIntoView() {}

void AiOverlayDialogPageHandler::OnAnnotationAgentDisconnected() {}
