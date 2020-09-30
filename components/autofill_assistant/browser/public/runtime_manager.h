// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_MANAGER_H_

#include "components/autofill_assistant/browser/public/runtime_observer.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
// Notifies subscribed observers when the UI state changes.
// Other components can query autofill assistant UI state by accessing
// Manager through WebContents.
class RuntimeManager {
 public:
  // Returns the instance of RuntimeManager that was attached to the
  // specified WebContents. Creates a new instance if no instance was attached
  // to |contents| yet.
  static RuntimeManager* GetForWebContents(content::WebContents* contents);

  // Add/Remove observers
  virtual void AddObserver(RuntimeObserver* observer) = 0;
  virtual void RemoveObserver(RuntimeObserver* observer) = 0;

  // Return Autofill Assistant state.
  virtual UIState GetState() const = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_MANAGER_H_
