// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_MANAGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/autofill_assistant/browser/public/runtime_observer.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

// TODO: Move implementation to internal/. When that is done, the public
// dependency on content for the build target autofill_assistant/browser:public
// can be removed.
namespace autofill_assistant {
class RuntimeManagerImpl
    : public RuntimeManager,
      public content::WebContentsUserData<RuntimeManagerImpl> {
 public:
  // Returns the instance of RuntimeManagerImpl that was attached to the
  // specified WebContents. Creates new instance if it doesn't exist yet.
  static RuntimeManagerImpl* GetForWebContents(content::WebContents* contents);

  ~RuntimeManagerImpl() override;
  RuntimeManagerImpl(const RuntimeManagerImpl&) = delete;
  RuntimeManagerImpl& operator=(const RuntimeManagerImpl&) = delete;

  // From RuntimeManager:
  void AddObserver(RuntimeObserver* observer) override;
  void RemoveObserver(RuntimeObserver* observer) override;
  UIState GetState() const override;
  void SetUIState(UIState state) override;
  base::WeakPtr<RuntimeManager> GetWeakPtr() override;

 private:
  friend class content::WebContentsUserData<RuntimeManagerImpl>;

  explicit RuntimeManagerImpl(content::WebContents* web_contents);

  // Holds the state of Autofill Assistant.
  UIState ui_state_ = UIState::kNotShown;

  // Observers of Autofill Assistant's state.
  base::ObserverList<RuntimeObserver> observers_;
  base::WeakPtrFactory<RuntimeManagerImpl> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_MANAGER_IMPL_H_
