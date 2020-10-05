// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if !defined(OS_CHROMEOS)
#include "base/check_op.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/component_updater/soda_en_us_component_installer.h"
#include "chrome/browser/component_updater/soda_ja_jp_component_installer.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int GetDownloadProgress(
    std::unordered_map<std::string, update_client::CrxUpdateItem>
        downloading_components) {
  int total_bytes = 0;
  int downloaded_bytes = 0;

  for (auto component : downloading_components) {
    if (component.second.downloaded_bytes >= 0 &&
        component.second.total_bytes > 0) {
      downloaded_bytes += component.second.downloaded_bytes;
      total_bytes += component.second.total_bytes;
    }
  }

  if (total_bytes == 0)
    return -1;

  DCHECK_LE(downloaded_bytes, total_bytes);
  return 100 *
         base::ClampToRange(double{downloaded_bytes} / total_bytes, 0.0, 1.0);
}

}  // namespace

#endif  // !defined(OS_CHROMEOS)

namespace settings {

#if defined(OS_CHROMEOS)
AccessibilityMainHandler::AccessibilityMainHandler() = default;
#else
AccessibilityMainHandler::AccessibilityMainHandler(PrefService* prefs)
    : prefs_(prefs) {}
#endif  // defined(OS_CHROMEOS)

AccessibilityMainHandler::~AccessibilityMainHandler() = default;

void AccessibilityMainHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "a11yPageReady",
      base::BindRepeating(&AccessibilityMainHandler::HandleA11yPageReady,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "confirmA11yImageLabels",
      base::BindRepeating(
          &AccessibilityMainHandler::HandleCheckAccessibilityImageLabels,
          base::Unretained(this)));
}

void AccessibilityMainHandler::OnJavascriptAllowed() {
#if defined(OS_CHROMEOS)
  accessibility_subscription_ =
      chromeos::AccessibilityManager::Get()->RegisterCallback(
          base::BindRepeating(
              &AccessibilityMainHandler::OnAccessibilityStatusChanged,
              base::Unretained(this)));
#else
  component_updater_observer_.Add(g_browser_process->component_updater());
#endif  // defined(OS_CHROMEOS)
}

void AccessibilityMainHandler::OnJavascriptDisallowed() {
#if defined(OS_CHROMEOS)
  accessibility_subscription_.reset();
#else
  component_updater_observer_.RemoveAll();
#endif  // defined(OS_CHROMEOS)
}

void AccessibilityMainHandler::HandleA11yPageReady(
    const base::ListValue* args) {
  AllowJavascript();
  SendScreenReaderStateChanged();
}

void AccessibilityMainHandler::HandleCheckAccessibilityImageLabels(
    const base::ListValue* args) {
  // When the user tries to enable the feature, show the modal dialog. The
  // dialog will disable the feature again if it is not accepted.
  content::WebContents* web_contents = web_ui()->GetWebContents();
  content::RenderWidgetHostView* view =
      web_contents->GetRenderViewHost()->GetWidget()->GetView();
  gfx::Rect rect = view->GetViewBounds();
  auto model = std::make_unique<AccessibilityLabelsBubbleModel>(
      Profile::FromWebUI(web_ui()), web_contents, true /* enable always */);
  chrome::ShowConfirmBubble(
      web_contents->GetTopLevelNativeWindow(), view->GetNativeView(),
      gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
}

void AccessibilityMainHandler::SendScreenReaderStateChanged() {
  base::Value result(accessibility_state_utils::IsScreenReaderEnabled());
  FireWebUIListener("screen-reader-state-changed", result);
}

#if defined(OS_CHROMEOS)
void AccessibilityMainHandler::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    SendScreenReaderStateChanged();
  }
}
#else
void AccessibilityMainHandler::OnEvent(Events event, const std::string& id) {
  if (id != component_updater::SODAComponentInstallerPolicy::GetExtensionId() &&
      id != component_updater::SodaEnUsComponentInstallerPolicy::
                GetExtensionId() &&
      id !=
          component_updater::SodaJaJpComponentInstallerPolicy::GetExtensionId())
    return;

  switch (event) {
    case Events::COMPONENT_UPDATE_FOUND:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
    case Events::COMPONENT_UPDATE_UPDATING: {
      update_client::CrxUpdateItem item;
      g_browser_process->component_updater()->GetComponentDetails(id, &item);
      downloading_components_[id] = item;
      const int progress = GetDownloadProgress(downloading_components_);
      // When GetDownloadProgress returns -1, do nothing. It returns -1 when the
      // downloaded or total bytes is unknown.
      if (progress != -1) {
        FireWebUIListener(
            "enable-live-caption-subtitle-changed",
            base::Value(l10n_util::GetStringFUTF16Int(
                IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_PROGRESS,
                progress)));
      }
    } break;
    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_NOT_UPDATED:
      FireWebUIListener(
          "enable-live-caption-subtitle-changed",
          base::Value(l10n_util::GetStringUTF16(
              IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_COMPLETE)));
      break;
    case Events::COMPONENT_UPDATE_ERROR:
      prefs_->SetBoolean(prefs::kLiveCaptionEnabled, false);
      FireWebUIListener(
          "enable-live-caption-subtitle-changed",
          base::Value(l10n_util::GetStringUTF16(
              IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR)));
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
      // Do nothing.
      break;
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
