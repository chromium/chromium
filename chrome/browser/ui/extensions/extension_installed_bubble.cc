// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_bubble.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::Extension;

namespace {

// How long to wait for browser action animations to complete before retrying.
const int kAnimationWaitMs = 50;
// How often we retry when waiting for browser action animation to end.
const int kAnimationWaitRetries = 10;

// Class responsible for showing the bubble after it's installed. Owns itself.
class ExtensionInstalledBubbleObserver
    : public BrowserListObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionInstalledBubbleObserver(
      scoped_refptr<const extensions::Extension> extension,
      Browser* browser,
      const SkBitmap& icon)
      : extension_(extension),
        browser_(browser),
        icon_(icon),
        animation_wait_retries_(0) {
    // |extension| has been initialized but not loaded at this point. We need to
    // wait on showing the Bubble until the EXTENSION_LOADED gets fired.
    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(browser->profile()));
    BrowserList::AddObserver(this);
  }

  void Run() { OnExtensionLoaded(nullptr, extension_.get()); }

 private:
  ~ExtensionInstalledBubbleObserver() override {
    BrowserList::RemoveObserver(this);
  }

  // BrowserListObserver:
  void OnBrowserClosing(Browser* browser) override {
    if (browser_ == browser) {
      // Browser is closing before the bubble was shown.
      // TODO(hcarmona): Look into logging this with the BubbleManager.
      delete this;
    }
  }

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    if (extension == extension_.get()) {
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      // Only then can we be sure that a BrowserAction or PageAction has had
      // views created which we can inspect for the purpose of previewing of
      // pointing to them.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&ExtensionInstalledBubbleObserver::Initialize,
                         weak_factory_.GetWeakPtr()));
    }
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (extension == extension_.get()) {
      // Extension is going away.
      delete this;
    }
  }

  void Initialize() {
    bubble_ =
        std::make_unique<ExtensionInstalledBubble>(extension_, browser_, icon_);
    Show();
  }

  // Called internally via PostTask to show the bubble.
  void Show() {
    DCHECK(bubble_);
    // TODO(hcarmona): Investigate having the BubbleManager query the bubble
    // for |ShouldShow|. This is important because the BubbleManager may decide
    // to delay showing the bubble.
    if (bubble_->ShouldShow()) {
      // Must be 2 lines because the manager will take ownership of bubble.
      BubbleManager* manager = browser_->GetBubbleManager();
      manager->ShowBubble(std::move(bubble_));
      delete this;
      return;
    }
    if (animation_wait_retries_++ < kAnimationWaitRetries) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ExtensionInstalledBubbleObserver::Show,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kAnimationWaitMs));
    } else {
      // Retries are over; won't try again.
      // TODO(hcarmona): Look into logging this with the BubbleManager.
      delete this;
    }
  }

  scoped_refptr<const extensions::Extension> extension_;
  Browser* const browser_;
  SkBitmap icon_;

  // The bubble that will be shown when the extension has finished installing.
  std::unique_ptr<ExtensionInstalledBubble> bubble_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  // The number of times to retry showing the bubble if |browser_'s| toolbar
  // is animating.
  int animation_wait_retries_;

  base::WeakPtrFactory<ExtensionInstalledBubbleObserver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleObserver);
};

// Returns the keybinding for an extension command, or a null if none exists.
std::unique_ptr<extensions::Command> GetCommand(
    const std::string& extension_id,
    Profile* profile,
    ExtensionInstalledBubble::BubbleType type) {
  std::unique_ptr<extensions::Command> result;
  extensions::Command command;
  extensions::CommandService* command_service =
      extensions::CommandService::Get(profile);
  bool has_command = false;
  if (type == ExtensionInstalledBubble::BROWSER_ACTION) {
    has_command = command_service->GetBrowserActionCommand(
        extension_id, extensions::CommandService::ACTIVE, &command, nullptr);
  } else if (type == ExtensionInstalledBubble::PAGE_ACTION) {
    has_command = command_service->GetPageActionCommand(
        extension_id, extensions::CommandService::ACTIVE, &command, nullptr);
  }
  if (has_command)
    result = std::make_unique<extensions::Command>(command);
  return result;
}

const extensions::ActionInfo* GetActionInfoForExtension(
    const extensions::Extension* extension) {
  const extensions::ActionInfo* action_info =
      extensions::ActionInfo::GetBrowserActionInfo(extension);

  if (!action_info)
    action_info = extensions::ActionInfo::GetPageActionInfo(extension);

  return action_info;
}

ExtensionInstalledBubble::BubbleType GetBubbleTypeForExtension(
    const extensions::Extension* extension,
    const extensions::ActionInfo* action_info) {
  if (action_info && action_info->type == extensions::ActionInfo::TYPE_BROWSER)
    return ExtensionInstalledBubble::BubbleType::BROWSER_ACTION;
  if (action_info && action_info->type == extensions::ActionInfo::TYPE_PAGE)
    return ExtensionInstalledBubble::BubbleType::PAGE_ACTION;
  if (!extensions::OmniboxInfo::GetKeyword(extension).empty())
    return ExtensionInstalledBubble::BubbleType::OMNIBOX_KEYWORD;
  return ExtensionInstalledBubble::BubbleType::GENERIC;
}

int GetOptionsForExtension(const extensions::Extension& extension,
                           const Browser& browser,
                           const extensions::ActionInfo* action_info,
                           ExtensionInstalledBubble::BubbleType type,
                           bool has_action_command) {
  int options = 0;
  if (extensions::sync_helper::IsSyncable(&extension) &&
      SyncPromoUI::ShouldShowSyncPromo(browser.profile()))
    options |= ExtensionInstalledBubble::SIGN_IN_PROMO;

  // Determine the bubble options we want, based on the extension type.
  switch (type) {
    case ExtensionInstalledBubble::BROWSER_ACTION:
    case ExtensionInstalledBubble::PAGE_ACTION:
      DCHECK(action_info);
      if (!action_info->synthesized)
        options |= ExtensionInstalledBubble::HOW_TO_USE;

      if (has_action_command) {
        options |= ExtensionInstalledBubble::SHOW_KEYBINDING;
      } else {
        // The How-To-Use text makes the bubble seem a little crowded when the
        // extension has a keybinding, so the How-To-Manage text is not shown
        // in those cases.
        options |= ExtensionInstalledBubble::HOW_TO_MANAGE;
      }
      break;
    case ExtensionInstalledBubble::OMNIBOX_KEYWORD:
      options |= ExtensionInstalledBubble::HOW_TO_USE |
                 ExtensionInstalledBubble::HOW_TO_MANAGE;
      break;
    case ExtensionInstalledBubble::GENERIC:
      break;
  }

  return options;
}

ExtensionInstalledBubble::AnchorPosition GetAnchorPositionForType(
    ExtensionInstalledBubble::BubbleType type) {
  if (type == ExtensionInstalledBubble::BROWSER_ACTION ||
      type == ExtensionInstalledBubble::PAGE_ACTION) {
    return ExtensionInstalledBubble::ANCHOR_ACTION;
  }
  if (type == ExtensionInstalledBubble::OMNIBOX_KEYWORD)
    return ExtensionInstalledBubble::ANCHOR_OMNIBOX;
  return ExtensionInstalledBubble::ANCHOR_APP_MENU;
}

}  // namespace

// static
void ExtensionInstalledBubble::ShowBubble(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    const SkBitmap& icon) {
  // The ExtensionInstalledBubbleObserver will delete itself when the
  // ExtensionInstalledBubble is shown or when it can't be shown anymore.
  auto* observer =
      new ExtensionInstalledBubbleObserver(extension, browser, icon);
  extensions::ExtensionRegistry* reg =
      extensions::ExtensionRegistry::Get(browser->profile());
  if (reg->enabled_extensions().GetByID(extension->id())) {
    observer->Run();
  }
}

ExtensionInstalledBubble::ExtensionInstalledBubble(
    scoped_refptr<const Extension> extension,
    Browser* browser,
    const SkBitmap& icon)
    : ExtensionInstalledBubble(extension,
                               browser,
                               icon,
                               GetActionInfoForExtension(extension.get())) {}

ExtensionInstalledBubble::ExtensionInstalledBubble(
    scoped_refptr<const Extension> extension,
    Browser* browser,
    const SkBitmap& icon,
    const extensions::ActionInfo* action_info)
    : extension_(extension),
      browser_(browser),
      icon_(icon),
      type_(GetBubbleTypeForExtension(extension_.get(), action_info)),
      action_command_(GetCommand(extension_->id(), browser_->profile(), type_)),
      options_(GetOptionsForExtension(*extension.get(),
                                      *browser,
                                      action_info,
                                      type_,
                                      has_command_keybinding())),
      anchor_position_(GetAnchorPositionForType(type_)) {}

ExtensionInstalledBubble::~ExtensionInstalledBubble() {}

bool ExtensionInstalledBubble::ShouldClose(BubbleCloseReason reason) const {
  // Installing an extension triggers a navigation event that should be ignored.
  return reason != BUBBLE_CLOSE_NAVIGATED;
}

std::string ExtensionInstalledBubble::GetName() const {
  return "ExtensionInstalled";
}

const content::RenderFrameHost* ExtensionInstalledBubble::OwningFrame() const {
  return nullptr;
}

base::string16 ExtensionInstalledBubble::GetHowToUseDescription() const {
  int message_id = 0;
  base::string16 extra;
  if (action_command_)
    extra = action_command_->accelerator().GetShortcutText();

  switch (type_) {
    case BROWSER_ACTION:
      message_id = extra.empty() ? IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO :
          IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO_WITH_SHORTCUT;
      break;
    case PAGE_ACTION:
      message_id = extra.empty() ? IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO :
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO_WITH_SHORTCUT;
      break;
    case OMNIBOX_KEYWORD:
      extra =
          base::UTF8ToUTF16(extensions::OmniboxInfo::GetKeyword(extension()));
      message_id = IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO;
      break;
    case GENERIC:
      break;
  }

  if (message_id == 0)
    return base::string16();
  return extra.empty() ? l10n_util::GetStringUTF16(message_id) :
      l10n_util::GetStringFUTF16(message_id, extra);
}
