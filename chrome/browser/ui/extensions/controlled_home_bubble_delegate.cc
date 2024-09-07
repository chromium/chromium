// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/controlled_home_bubble_delegate.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/referrer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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
          ControlledHomeBubbleDelegate::kAcknowledgedPreference,
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
      extension_id, ControlledHomeBubbleDelegate::kAcknowledgedPreference,
      base::Value(true));
}

}  // namespace

ControlledHomeBubbleDelegate::ControlledHomeBubbleDelegate(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      extension_(GetExtensionToWarnAbout(*profile_)) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
}

ControlledHomeBubbleDelegate::~ControlledHomeBubbleDelegate() {
  GetPendingProfileSet().erase(profile_);
}

base::AutoReset<bool>
ControlledHomeBubbleDelegate::IgnoreLearnMoreForTesting() {
  return base::AutoReset<bool>(&g_should_ignore_learn_more_for_testing, true);
}

void ControlledHomeBubbleDelegate::ClearProfileSetForTesting() {
  GetShownProfileSet().clear();
}

bool ControlledHomeBubbleDelegate::ShouldShow() {
  // Show if there's a non-acknowledged controlling extension and we haven't
  // shown (and aren't about to show in a pending bubble) for this profile.
  return extension_ && GetShownProfileSet().count(profile_) == 0u &&
         GetPendingProfileSet().count(profile_) == 0u;
}

void ControlledHomeBubbleDelegate::PendingShow() {
  DCHECK_EQ(0u, GetPendingProfileSet().count(profile_));
  // Mark the profile as having a pending bubble. This way, we won't queue up
  // another bubble if one is waiting for animation.
  GetPendingProfileSet().insert(profile_);
}

std::u16string ControlledHomeBubbleDelegate::GetHeadingText() {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_TITLE_HOME_PAGE_BUBBLE);
}

std::u16string ControlledHomeBubbleDelegate::GetBodyText(
    bool anchored_to_action) {
  const extensions::SettingsOverrides* settings =
      extensions::SettingsOverrides::Get(extension_.get());
  CHECK(settings);

  bool startup_change = !settings->startup_pages.empty();
  bool search_change = settings->search_engine.has_value();

  std::u16string body;
  int first_line_id =
      anchored_to_action
          ? IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE_SPECIFIC
          : IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE;
  int second_line_id = 0;
  if (startup_change && search_change) {
    second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_SEARCH;
  } else if (startup_change) {
    second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES;
  } else if (search_change) {
    second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE;
  }
  DCHECK_NE(0, first_line_id);
  body = anchored_to_action
             ? l10n_util::GetStringUTF16(first_line_id)
             : l10n_util::GetStringFUTF16(
                   first_line_id, base::UTF8ToUTF16(extension_->name()));
  if (second_line_id) {
    body += l10n_util::GetStringUTF16(second_line_id);
  }

  body += l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_THIRD_LINE_CONFIRMATION);

  return body;
}

std::u16string ControlledHomeBubbleDelegate::GetActionButtonText() {
  // An empty string is returned so that we don't display the button prompting
  // to remove policy-installed extensions.
  if (IsPolicyIndicationNeeded()) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

std::u16string ControlledHomeBubbleDelegate::GetDismissButtonText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

ui::mojom::DialogButton ControlledHomeBubbleDelegate::GetDefaultDialogButton() {
  // TODO(estade): we should set a default where appropriate. See
  // http://crbug.com/751279
  return ui::mojom::DialogButton::kNone;
}

std::string ControlledHomeBubbleDelegate::GetAnchorActionId() {
  return extension_->id();
}

void ControlledHomeBubbleDelegate::OnBubbleShown(
    base::OnceClosure close_bubble_callback) {
  DCHECK_EQ(0u, GetShownProfileSet().count(profile_));
  DCHECK_EQ(1u, GetPendingProfileSet().count(profile_));

  GetShownProfileSet().insert(profile_);
  GetPendingProfileSet().erase(profile_);
  close_bubble_callback_ = std::move(close_bubble_callback);

  // It's possible the extension was removed while the bubble was getting ready
  // to show. If that happens, close the bubble "immediately" (after a post
  // task) when it's shown. We post a task just so we don't enter a CloseWidget
  // cycle in the same series as it being shown.
  if (!extension_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(close_bubble_callback_));
  }
}

void ControlledHomeBubbleDelegate::OnBubbleClosed(CloseAction action) {
  // OnBubbleClosed() can be called twice when we receive multiple
  // "OnWidgetDestroying" notifications (this can at least happen when we close
  // a window with a notification open). Handle this gracefully.
  if (close_action_) {
    DCHECK(close_action_ == CLOSE_DISMISS_USER_ACTION ||
           close_action_ == CLOSE_DISMISS_DEACTIVATION);
    return;
  }

  close_action_ = action;
  extension_registry_observation_.Reset();

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
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->DisableExtension(extension_->id(),
                             extensions::disable_reason::DISABLE_USER_ACTION);
      break;
    case CLOSE_LEARN_MORE: {
      AcknowledgeExtension(*profile_, extension_->id());
      if (!g_should_ignore_learn_more_for_testing) {
        GURL learn_more_url(chrome::kExtensionControlledSettingLearnMoreURL);
        DCHECK(learn_more_url.is_valid());
        browser_->OpenURL(
            content::OpenURLParams(learn_more_url, content::Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_LINK, false),
            /*navigation_handle_callback=*/{});
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

std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
ControlledHomeBubbleDelegate::GetExtraViewInfo() {
  auto extra_view_info = std::make_unique<ExtraViewInfo>();

  if (IsPolicyIndicationNeeded()) {
    extra_view_info->resource = &vector_icons::kBusinessIcon;
    extra_view_info->text =
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN);
    extra_view_info->is_learn_more = false;
  } else {
    extra_view_info->text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
    extra_view_info->is_learn_more = true;
  }

  return extra_view_info;
}

bool ControlledHomeBubbleDelegate::IsPolicyIndicationNeeded() const {
  return extensions::Manifest::IsPolicyLocation(extension_->location());
}

void ControlledHomeBubbleDelegate::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  HandleExtensionUnloadOrUninstall(extension);
}

void ControlledHomeBubbleDelegate::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  HandleExtensionUnloadOrUninstall(extension);
}

void ControlledHomeBubbleDelegate::HandleExtensionUnloadOrUninstall(
    const extensions::Extension* extension) {
  if (extension != extension_) {
    return;
  }

  // Null out `extension_` to indicate it was removed.
  extension_ = nullptr;

  // If the callback is set, then that means that OnShown() was called, and the
  // bubble was displayed. Close it, since the extension is gone.
  if (close_bubble_callback_) {
    std::move(close_bubble_callback_).Run();
  }
}

void ControlledHomeBubbleDelegate::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  // It is possible that the extension registry is destroyed before the
  // controller. In such case, the controller should no longer observe the
  // registry.
  DCHECK(extension_registry_observation_.IsObservingSource(registry));
  extension_registry_observation_.Reset();
}
