// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_util.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kAboutPageUrl[] =
    "https://www.google.com/chrome/devices/chromecast/";
constexpr char kCastLearnMorePageUrl[] =
    "https://support.google.com/chromecast/answer/2998338";
constexpr char kCastHelpCenterPageUrl[] =
    "https://support.google.com/chromecast/"
    "topic/3447927";
}  // namespace

// static
void CastToolbarButtonUtil::AddCastChildActions(
    actions::ActionItem* cast_action,
    Browser* browser) {
  cast_action->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ShowSingletonTab(browser, GURL(kAboutPageUrl));
              },
              base::Unretained(browser)))
          .SetActionId(kActionMediaRouterAbout)
          .SetText(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ABOUT))
          .Build());

  cast_action->AddChild(actions::ActionItem::Builder(
                            base::BindRepeating(
                                [](Browser* browser, actions::ActionItem* item,
                                   actions::ActionInvocationContext context) {
                                  ShowSingletonTab(browser,
                                                   GURL(kCastLearnMorePageUrl));
                                },
                                base::Unretained(browser)))
                            .SetActionId(kActionMediaRouterLearnMore)
                            .SetText(l10n_util::GetStringUTF16(IDS_LEARN_MORE))
                            .Build());

  cast_action->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ShowSingletonTab(browser, GURL(kCastHelpCenterPageUrl));
                base::RecordAction(
                    base::UserMetricsAction("MediaRouter_Ui_Navigate_Help"));
              },
              base::Unretained(browser)))
          .SetActionId(kActionMediaRouterHelp)
          .SetText(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_HELP))
          .Build());

  cast_action->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* pref_service = browser->profile()->GetPrefs();
                bool checked = !pref_service->GetBoolean(
                    media_router::prefs::kMediaRouterMediaRemotingEnabled);
                pref_service->SetBoolean(
                    media_router::prefs::kMediaRouterMediaRemotingEnabled,
                    checked);
              },
              base::Unretained(browser)))
          .SetActionId(kActionMediaRouterToggleMediaRemoting)
          .SetText(
              l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING))
          .SetChecked(browser->profile()->GetPrefs()->GetBoolean(
              media_router::prefs::kMediaRouterMediaRemotingEnabled))
          .Build());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  cast_action->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ShowSingletonTab(browser,
                                 GURL(chrome::kChromeUICastFeedbackURL));
              },
              base::Unretained(browser)))
          .SetActionId(kActionMediaToolbarContextReportCastIssue)
          .SetText(l10n_util::GetStringUTF16(
              IDS_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE))
          .Build());
#endif
}
