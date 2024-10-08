// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_WEB_CONTENTS_MANAGER_H_

#include <cstdint>

#include "base/component_export.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
namespace content {

class WebContents;

}  // namespace content

namespace chromeos {

// `MahiWebContentsManager` is the central class for mahi web contents in the
// browser responsible for:
// 1. Being the single source of truth for mahi browser parameters like focused
//    web page, the lasted extracted web page, etc. and providing this
//    information to ChromeOS backend as needed.
// 2. Decides the distillability of a web page and extract contents from a
//    requested web page.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiWebContentsManager {
 public:
  MahiWebContentsManager(const MahiWebContentsManager&) = delete;
  MahiWebContentsManager& operator=(const MahiWebContentsManager&) = delete;
  virtual ~MahiWebContentsManager();

  static MahiWebContentsManager* Get();

  // Called when the focused tab finish loading.
  // Virtual so we can override in tests.
  virtual void OnFocusedPageLoadComplete(
      content::WebContents* web_contents) = 0;

  // Clears the focused web content state, and notifies mahi manager.
  // Passes the `top_level_window` downstream if it is set. This may suppress
  // the notification if it is a media app window that is observed by media app
  // content manager.
  virtual void ClearFocusedWebContentState() = 0;

  // Clears the focused web content and its state if the focused content is
  // destroyed.
  virtual void WebContentsDestroyed(content::WebContents* web_contents) = 0;

  // Called when the browser context menu has been clicked by the user.
  // `question` is used only if `ButtonType` is kQA.
  // Virtual so we can override in tests.
  virtual void OnContextMenuClicked(int64_t display_id,
                                    mahi::ButtonType button_type,
                                    const std::u16string& question,
                                    const gfx::Rect& mahi_menu_bounds) = 0;

  // Returns boolean to indicate if the current focused page is distillable. The
  // default return is false, for the cases when the focused page's
  // distillability has not been checked yet.
  virtual bool IsFocusedPageDistillable() = 0;

  // Callback function of the user request from the OS side to get the page
  // content.
  // Virtual so we can override in tests.
  virtual void RequestContent(const base::UnguessableToken& page_id,
                              mahi::GetContentCallback callback) = 0;

 protected:
  MahiWebContentsManager();
};

// A scoped object that overrides `chromeos::MahiWebContentsManager::Get()` to
// the provided object pointer, useful for testing and mocking.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ScopedMahiWebContentsManagerOverride {
 public:
  explicit ScopedMahiWebContentsManagerOverride(
      MahiWebContentsManager* manager);
  ScopedMahiWebContentsManagerOverride(
      const ScopedMahiWebContentsManagerOverride&) = delete;
  ScopedMahiWebContentsManagerOverride& operator=(
      const ScopedMahiWebContentsManagerOverride&) = delete;
  ~ScopedMahiWebContentsManagerOverride();

 private:
  static ScopedMahiWebContentsManagerOverride* instance_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_WEB_CONTENTS_MANAGER_H_
