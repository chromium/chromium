// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_overview_tracing/arc_overview_tracing_ui.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/overview_tracing_handler.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

constexpr char kArcOverviewTracingJsPath[] = "arc_overview_tracing.js";
constexpr char kArcOverviewTracingUiJsPath[] = "arc_overview_tracing_ui.js";
constexpr char kArcTracingUiJsPath[] = "arc_tracing_ui.js";
constexpr char kArcTracingCssPath[] = "arc_tracing.css";
constexpr char kJavascriptDomain[] = "cr.ArcOverviewTracing.";

void CreateAndAddOverviewDataSource(Profile* profile) {
  content::WebUIDataSource* const source =
      content::WebUIDataSource::CreateAndAdd(
          profile, chrome::kChromeUIArcOverviewTracingHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_ARC_OVERVIEW_TRACING_HTML);
  source->AddResourcePath(kArcOverviewTracingJsPath,
                          IDR_ARC_OVERVIEW_TRACING_JS);
  source->AddResourcePath(kArcOverviewTracingUiJsPath,
                          IDR_ARC_OVERVIEW_TRACING_UI_JS);
  source->AddResourcePath(kArcTracingCssPath, IDR_ARC_TRACING_CSS);
  source->AddResourcePath(kArcTracingUiJsPath, IDR_ARC_TRACING_UI_JS);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");

  base::Value::Dict localized_strings;
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);
  source->AddLocalizedStrings(localized_strings);
}

using Result = arc::OverviewTracingHandler::Result;

std::unique_ptr<Result> LoadGraphicsModel(const std::string& json_text) {
  arc::ArcTracingGraphicsModel graphics_model;
  graphics_model.set_skip_structure_validation();
  if (!graphics_model.LoadFromJson(json_text)) {
    return std::make_unique<Result>(base::Value(), base::FilePath(),
                                    "Failed to load tracing model");
  }

  base::Value::Dict model = graphics_model.Serialize();
  return std::make_unique<Result>(base::Value(std::move(model)),
                                  base::FilePath(), "Tracing model is loaded");
}

class Handler : public content::WebUIMessageHandler, public ui::EventHandler {
 public:
  Handler() : weak_ptr_factory_(this) {
    tracing_ =
        std::make_unique<arc::OverviewTracingHandler>(base::BindRepeating(
            &Handler::OnArcWindowFocusChange, weak_ptr_factory_.GetWeakPtr()));

    tracing_->set_start_build_model_cb(
        base::BindRepeating(&Handler::SetStatus, weak_ptr_factory_.GetWeakPtr(),
                            "Building model..."));
    tracing_->set_graphics_model_ready_cb(base::BindRepeating(
        &Handler::OnGraphicsModelReady, weak_ptr_factory_.GetWeakPtr()));
  }

  ~Handler() override {
    // Delete this before the weak ptr factory so our callbacks can still be
    // invoked.
    tracing_.reset();
  }

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "loadFromText", base::BindRepeating(&Handler::HandleLoadFromText,
                                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setMaxTime", base::BindRepeating(&Handler::HandleSetMaxTime,
                                          base::Unretained(this)));
  }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    DCHECK(arc_active_window_);

    // Only two flags (decorators) must be on.
    constexpr int kFlags = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;
    // Four flags must be off (avoids future conflict, and prevents long
    // press from double-activating).
    constexpr int kMask = kFlags | ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN |
                          ui::EF_ALT_DOWN | ui::EF_IS_REPEAT;

    if (event->type() != ui::EventType::kKeyPressed ||
        event->key_code() != ui::VKEY_G || (event->flags() & kMask) != kFlags) {
      return;
    }
    if (tracing_->is_tracing()) {
      tracing_->StopTracing();
    } else {
      SetStatus("Collecting samples...");
      tracing_->StartTracing(file_manager::util::GetDownloadsFolderForProfile(
                                 Profile::FromWebUI(web_ui())),
                             max_tracing_time_);
    }
  }

 private:
  void HandleLoadFromText(const base::Value::List& args) {
    DCHECK_EQ(1U, args.size());
    if (!args[0].is_string()) {
      LOG(ERROR) << "Invalid input";
      return;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&LoadGraphicsModel, args[0].GetString()),
        base::BindOnce(&Handler::OnGraphicsModelReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetStatus(std::string_view status) {
    AllowJavascript();
    CallJavascriptFunction(kJavascriptDomain + std::string("setStatus"),
                           base::Value(status.empty() ? "Idle" : status));
  }

  void ActivateWebUIWindow() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto* const window = web_ui()->GetWebContents()->GetTopLevelNativeWindow();
    if (!window) {
      LOG(ERROR) << "Failed to activate, no top level window.";
      return;
    }

    platform_util::ActivateWindow(window);
  }

  void OnGraphicsModelReady(
      std::unique_ptr<arc::OverviewTracingHandler::Result> result) {
    SetStatus(result->status);

    if (!result->model.is_dict()) {
      return;
    }

    CallJavascriptFunction(kJavascriptDomain + std::string("setModel"),
                           std::move(result->model));

    // If we are running in response to a window activation from within an
    // observer, activating the web UI immediately will cause a DCHECK failure.
    // Post as a UI task so we activate the web UI after the observer has
    // returned.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&Handler::ActivateWebUIWindow,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void OnArcWindowFocusChange(aura::Window* window) {
    if (arc_active_window_) {
      arc_active_window_->RemovePreTargetHandler(this);
    }
    arc_active_window_ = window;
    if (arc_active_window_) {
      arc_active_window_->AddPreTargetHandler(this);
    }
  }

  void HandleSetMaxTime(const base::Value::List& args) {
    if (args.size() != 1) {
      LOG(ERROR) << "Expect 1 numeric arg";
      return;
    }

    auto new_time = args[0].GetIfDouble();
    if (!new_time.has_value() || *new_time < 1.0) {
      LOG(ERROR) << "Interval too small or not a number: " << args[0];
      return;
    }

    max_tracing_time_ = base::Seconds(*new_time);
  }

  raw_ptr<aura::Window> arc_active_window_{nullptr};
  std::unique_ptr<arc::OverviewTracingHandler> tracing_;
  base::TimeDelta max_tracing_time_{base::Seconds(5)};

  base::WeakPtrFactory<Handler> weak_ptr_factory_;
};

}  // anonymous namespace

ArcOverviewTracingUIConfig::ArcOverviewTracingUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIArcOverviewTracingHost) {}

bool ArcOverviewTracingUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return arc::IsArcAllowedForProfile(
      Profile::FromBrowserContext(browser_context));
}

ArcOverviewTracingUI::ArcOverviewTracingUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<Handler>());
  CreateAndAddOverviewDataSource(Profile::FromWebUI(web_ui));
}

}  // namespace ash
