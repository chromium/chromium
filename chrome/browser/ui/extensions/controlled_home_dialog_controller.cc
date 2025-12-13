// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

namespace {

// Whether we should ignore learn more clicks.
bool g_should_ignore_learn_more_for_testing = false;

// The set of profiles for which a controlled home bubble has been shown (we
// only show once per profile per session).
std::set<Profile*>& GetShownProfileSet() {
  static base::NoDestructor<std::set<Profile*>> g_profiles;
  return *g_profiles;
}

// The set of profiles for which a bubble is pending (but hasn't yet shown).
std::set<Profile*>& GetPendingProfileSet() {
  static base::NoDestructor<std::set<Profile*>> g_profiles;
  return *g_profiles;
}

// Gets the extension that currently controls the home page and has not yet
// been acknowledged, if any.
const extensions::Extension* GetExtensionToWarnAbout(Profile& profile) {
  const extensions::Extension* controlling_extension =
      extensions::GetExtensionOverridingHomepage(&profile);
  if (!controlling_extension) {
    // No controlling extension; nothing to warn about.
    return nullptr;
  }

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(&profile);
  bool was_acknowledged = false;
  if (extension_prefs->ReadPrefAsBoolean(
          controlling_extension->id(),
          ControlledHomeDialogController::kAcknowledgedPreference,
          &was_acknowledged) &&
      was_acknowledged) {
    // Extension was already acknowledged.
    return nullptr;
  }

  return controlling_extension;
}

// Acknowledges the extension with the given `extension_id` so that we don't
// prompt the user about it again in the future.
void AcknowledgeExtension(Profile& profile,
                          const extensions::ExtensionId& extension_id) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(&profile);
  extension_prefs->UpdateExtensionPref(
      extension_id, ControlledHomeDialogController::kAcknowledgedPreference,
      base::Value(true));
}

}  // namespace

ControlledHomeDialogController::ControlledHomeDialogController(
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents->GetWeakPtr()),
      extension_(GetExtensionToWarnAbout(*profile_)) {}

ControlledHomeDialogController::~ControlledHomeDialogController() {
  GetPendingProfileSet().erase(profile_);
}

base::AutoReset<bool>
ControlledHomeDialogController::IgnoreLearnMoreForTesting() {
  return base::AutoReset<bool>(&g_should_ignore_learn_more_for_testing, true);
}

void ControlledHomeDialogController::ClearProfileSetForTesting() {
  GetShownProfileSet().clear();
}

bool ControlledHomeDialogController::ShouldShow() {
  // Show if there's a non-acknowledged controlling extension and we haven't
  // shown (and aren't about to show in a pending bubble) for this profile.
  return extension_ && GetShownProfileSet().count(profile_) == 0u &&
         GetPendingProfileSet().count(profile_) == 0u;
}

void ControlledHomeDialogController::PendingShow() {
  DCHECK_EQ(0u, GetPendingProfileSet().count(profile_));
  // Mark the profile as having a pending bubble. This way, we won't queue up
  // another bubble if one is waiting for animation.
  GetPendingProfileSet().insert(profile_);
}

std::u16string ControlledHomeDialogController::GetHeadingText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_CONTROLLED_HOME_DIALOG_TITLE);
}

std::u16string ControlledHomeDialogController::GetBodyText() {
  const extensions::SettingsOverrides* settings =
      extensions::SettingsOverrides::Get(extension_.get());
  CHECK(settings);

  bool startup_change = !settings->startup_pages.empty();
  bool search_change = settings->search_engine.has_value();
  int second_line_id = 0;
  if (startup_change && search_change) {
    second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_SEARCH;
  } else if (startup_change) {
    second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES;
  } else if (search_change) {
    second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE;
  }

  std::u16string body;
  body = l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_CONTROLLED_HOME_DIALOG_DESCRIPTION,
      extensions::util::GetFixupExtensionNameForUIDisplay(extension_->name()));

  if (second_line_id) {
    body += l10n_util::GetStringUTF16(second_line_id);
  }

  body += l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_THIRD_LINE_CONFIRMATION);

  return body;
}

std::u16string ControlledHomeDialogController::GetActionButtonText() {
  // An empty string is returned so that we don't display the button prompting
  // to remove policy-installed extensions.
  if (IsPolicyIndicationNeeded()) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

std::u16string ControlledHomeDialogController::GetDismissButtonText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

std::string ControlledHomeDialogController::GetAnchorActionId() {
  return extension_->id();
}

void ControlledHomeDialogController::OnBubbleShown() {
  DCHECK_EQ(0u, GetShownProfileSet().count(profile_));
  DCHECK_EQ(1u, GetPendingProfileSet().count(profile_));

  GetShownProfileSet().insert(profile_);
  GetPendingProfileSet().erase(profile_);
}

void ControlledHomeDialogController::OnBubbleClosed(CloseAction action) {
  // OnBubbleClosed() can be called twice when we receive multiple
  // "OnWidgetDestroying" notifications (this can at least happen when we close
  // a window with a notification open). Handle this gracefully.
  if (close_action_) {
    DCHECK(close_action_ == CLOSE_DISMISS_USER_ACTION ||
           close_action_ == CLOSE_DISMISS_DEACTIVATION);
    return;
  }

  close_action_ = action;

  if (action == CLOSE_DISMISS_DEACTIVATION) {
    return;  // Do nothing if the bubble was dismissed due to focus loss.
  }

  // We clear the profile set because the user chose to either remove, disable,
  // or acknowledge the extension. If they acknowledged it, we won't show the
  // bubble again, and in any other cases, we should re-show the bubble if
  // any extension goes back to overriding the home page (because it's contrary
  // to the user's choice).
  GetShownProfileSet().clear();

  switch (action) {
    case CLOSE_EXECUTE:
      // User clicked to disable the extension.
      extensions::ExtensionRegistrar::Get(profile_)->DisableExtension(
          extension_->id(), {extensions::disable_reason::DISABLE_USER_ACTION});
      break;
    case CLOSE_LEARN_MORE: {
      AcknowledgeExtension(*profile_, extension_->id());
      if (!g_should_ignore_learn_more_for_testing && web_contents_) {
        GURL learn_more_url(chrome::kExtensionControlledSettingLearnMoreURL);
        CHECK(learn_more_url.is_valid());
        content::OpenURLParams params(learn_more_url, content::Referrer(),
                                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                      ui::PAGE_TRANSITION_LINK,
                                      /*is_renderer_initiated=*/false);
        web_contents_->OpenURL(params, {});
      }
      break;
    }
    case CLOSE_DISMISS_USER_ACTION:
      AcknowledgeExtension(*profile_, extension_->id());
      break;
    case CLOSE_DISMISS_DEACTIVATION:
      NOTREACHED();  // This was handled above.
  }

  // Warning: |this| may be deleted here!
}

bool ControlledHomeDialogController::IsPolicyIndicationNeeded() const {
  return extensions::Manifest::IsPolicyLocation(extension_->location());
}
