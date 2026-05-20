// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_OBSERVER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/size.h"

class EmbeddedPermissionPromptObserver
    : public content::WebContentsUserData<EmbeddedPermissionPromptObserver> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the embedded permission prompt (PEPC) is shown, hidden (via
    // out of focus or choosing an option), or resized. `prompt_size` represents
    // the size bounds (width/height) of the prompt bubble, which is only valid
    // and non-empty if `is_showing` is true.
    virtual void OnEmbeddedPermissionPromptChanged(
        bool is_showing,
        const gfx::Size& prompt_size) = 0;
  };

  ~EmbeddedPermissionPromptObserver() override;

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  void NotifyEmbeddedPermissionPromptChanged(bool is_showing,
                                             const gfx::Size& prompt_size);

 private:
  friend class content::WebContentsUserData<EmbeddedPermissionPromptObserver>;
  explicit EmbeddedPermissionPromptObserver(content::WebContents* contents);

  base::ObserverList<Observer> observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_OBSERVER_H_
