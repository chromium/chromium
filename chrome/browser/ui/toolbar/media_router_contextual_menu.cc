// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"

#include <memory>
#include <string>

#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

// static
std::unique_ptr<MediaRouterContextualMenu> MediaRouterContextualMenu::Create(
    Browser* browser,
    Observer* observer) {
  return std::make_unique<MediaRouterContextualMenu>(
      browser,
      MediaRouterActionController::IsActionShownByPolicy(browser->profile()),
      observer);
}

MediaRouterContextualMenu::MediaRouterContextualMenu(Browser* browser,
                                                     bool shown_by_policy,
                                                     Observer* observer)
    : browser_(browser),
      observer_(observer),
      shown_by_policy_(shown_by_policy) {}

MediaRouterContextualMenu::~MediaRouterContextualMenu() = default;

std::unique_ptr<ui::SimpleMenuModel>
MediaRouterContextualMenu::CreateMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_ABOUT,
                                  IDS_MEDIA_ROUTER_ABOUT);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_LEARN_MORE, IDS_LEARN_MORE);
  menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_HELP, IDS_MEDIA_ROUTER_HELP);
  if (shown_by_policy_) {
    menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY,
                                    IDS_MEDIA_ROUTER_SHOWN_BY_POLICY);
    // TODO (kylixrd): Review the use of the hard-coded color constant.
    menu_model->SetIcon(
        menu_model->GetIndexOfCommandId(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY),
        ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                       gfx::kChromeIconGrey, 16));
  } else {
    menu_model->AddCheckItemWithStringId(
        IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION,
        IDS_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION);
  }
  menu_model->AddCheckItemWithStringId(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING,
                                       IDS_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING);
  if (!browser_->profile()->IsOffTheRecord()) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    if (browser_->profile()->GetPrefs()->GetBoolean(
            prefs::kUserFeedbackAllowed)) {
      menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_REPORT_ISSUE,
                                      IDS_MEDIA_ROUTER_REPORT_ISSUE);
    }
  }
  return menu_model;
}

bool MediaRouterContextualMenu::GetAlwaysShowActionPref() const {
  return MediaRouterActionController::GetAlwaysShowActionPref(
      browser_->profile());
}

void MediaRouterContextualMenu::SetAlwaysShowActionPref(bool always_show) {
  return MediaRouterActionController::SetAlwaysShowActionPref(
      browser_->profile(), always_show);
}

bool MediaRouterContextualMenu::IsCommandIdChecked(int command_id) const {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  switch (command_id) {
    case IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION:
      return GetAlwaysShowActionPref();
    case IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING:
      return pref_service->GetBoolean(
          media_router::prefs::kMediaRouterMediaRemotingEnabled);
    default:
      return false;
  }
}

bool MediaRouterContextualMenu::IsCommandIdEnabled(int command_id) const {
  return command_id != IDC_MEDIA_ROUTER_SHOWN_BY_POLICY;
}

bool MediaRouterContextualMenu::IsCommandIdVisible(int command_id) const {
  return true;
}

void MediaRouterContextualMenu::ExecuteCommand(int command_id,
                                               int event_flags) {
  const char kAboutPageUrl[] =
      "https://www.google.com/chrome/devices/chromecast/";
  const char kCastHelpCenterPageUrl[] =
      "https://support.google.com/chromecast/topic/3447927";
  const char kCastLearnMorePageUrl[] =
      "https://support.google.com/chromecast/answer/2998338";

  switch (command_id) {
    case IDC_MEDIA_ROUTER_ABOUT:
      ShowSingletonTab(browser_, GURL(kAboutPageUrl));
      break;
    case IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION:
      SetAlwaysShowActionPref(!GetAlwaysShowActionPref());
      break;
    case IDC_MEDIA_ROUTER_HELP:
      ShowSingletonTab(browser_, GURL(kCastHelpCenterPageUrl));
      base::RecordAction(base::UserMetricsAction(
          "MediaRouter_Ui_Navigate_Help"));
      break;
    case IDC_MEDIA_ROUTER_LEARN_MORE:
      ShowSingletonTab(browser_, GURL(kCastLearnMorePageUrl));
      break;
    case IDC_MEDIA_ROUTER_REPORT_ISSUE:
      ReportIssue();
      break;
    case IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING:
      ToggleMediaRemoting();
      break;
    default:
      NOTREACHED();
  }
}

void MediaRouterContextualMenu::OnMenuWillShow(ui::SimpleMenuModel* source) {
  observer_->OnContextMenuShown();
}

void MediaRouterContextualMenu::MenuClosed(ui::SimpleMenuModel* source) {
  observer_->OnContextMenuHidden();
}

void MediaRouterContextualMenu::ToggleMediaRemoting() {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled,
      !pref_service->GetBoolean(
          media_router::prefs::kMediaRouterMediaRemotingEnabled));
}

void MediaRouterContextualMenu::ReportIssue() {
  // Opens feedback page loaded from the media router extension.
  // This is temporary until feedback UI is redesigned.
  media_router::EventPageRequestManager* request_manager =
      media_router::EventPageRequestManagerFactory::GetApiForBrowserContext(
          browser_->profile());
  if (request_manager->media_route_provider_extension_id().empty())
    return;
  std::string feedback_url(
      extensions::kExtensionScheme +
      std::string(url::kStandardSchemeSeparator) +
      request_manager->media_route_provider_extension_id() + "/feedback.html");
  ShowSingletonTab(browser_, GURL(feedback_url));
}
