// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/cast/cast_contextual_menu.h"

#include <memory>
#include <string>

#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
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
std::unique_ptr<CastContextualMenu> CastContextualMenu::Create(
    Browser* browser,
    Observer* observer) {
  return std::make_unique<CastContextualMenu>(
      browser,
      CastToolbarButtonController::IsActionShownByPolicy(browser->profile()),
      observer);
}

CastContextualMenu::CastContextualMenu(Browser* browser,
                                                     bool shown_by_policy,
                                                     Observer* observer)
    : browser_(browser),
      observer_(observer),
      shown_by_policy_(shown_by_policy) {}

CastContextualMenu::~CastContextualMenu() = default;

std::unique_ptr<ui::SimpleMenuModel>
CastContextualMenu::CreateMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_ABOUT,
                                  IDS_MEDIA_ROUTER_ABOUT);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_LEARN_MORE, IDS_LEARN_MORE);
  menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_HELP, IDS_MEDIA_ROUTER_HELP);
  if (shown_by_policy_) {
    menu_model->AddItemWithStringId(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY,
                                    IDS_MEDIA_ROUTER_SHOWN_BY_POLICY);
    menu_model->SetIcon(
        menu_model->GetIndexOfCommandId(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY)
            .value(),
        ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                       ui::kColorIcon, 16));
  } else {
    menu_model->AddCheckItemWithStringId(
        IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION,
        IDS_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION);
  }

  menu_model->AddCheckItemWithStringId(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING,
                                       IDS_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (browser_->profile()->GetPrefs()->GetBoolean(
          prefs::kUserFeedbackAllowed)) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model->AddItemWithStringId(
        IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE,
        IDS_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  }
#endif

  return menu_model;
}

bool CastContextualMenu::GetAlwaysShowActionPref() const {
  return CastToolbarButtonController::GetAlwaysShowActionPref(
      browser_->profile());
}

void CastContextualMenu::SetAlwaysShowActionPref(bool always_show) {
  return CastToolbarButtonController::SetAlwaysShowActionPref(
      browser_->profile(), always_show);
}

bool CastContextualMenu::IsCommandIdChecked(int command_id) const {
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

bool CastContextualMenu::IsCommandIdEnabled(int command_id) const {
  return command_id != IDC_MEDIA_ROUTER_SHOWN_BY_POLICY;
}

bool CastContextualMenu::IsCommandIdVisible(int command_id) const {
  return true;
}

void CastContextualMenu::ExecuteCommand(int command_id,
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
      base::RecordAction(
          base::UserMetricsAction("MediaRouter_Ui_Navigate_Help"));
      break;
    case IDC_MEDIA_ROUTER_LEARN_MORE:
      ShowSingletonTab(browser_, GURL(kCastLearnMorePageUrl));
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE:
      ReportIssue();
      break;
#endif
    case IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING:
      ToggleMediaRemoting();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void CastContextualMenu::OnMenuWillShow(ui::SimpleMenuModel* source) {
  observer_->OnContextMenuShown();
}

void CastContextualMenu::MenuClosed(ui::SimpleMenuModel* source) {
  observer_->OnContextMenuHidden();
}

void CastContextualMenu::ToggleMediaRemoting() {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled,
      !pref_service->GetBoolean(
          media_router::prefs::kMediaRouterMediaRemotingEnabled));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void CastContextualMenu::ReportIssue() {
  ShowSingletonTab(
      browser_,
      GURL(base::StrCat({"chrome://", chrome::kChromeUICastFeedbackHost})));
}
#endif
