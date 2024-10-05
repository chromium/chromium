// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace user_annotations {

class UserAnnotationsService;

// A class that listens to events that may require persisting a user annotation,
// such as a form submission, and propagates them to the
// `UserAnnotationsKeyedService` for persistence.
class UserAnnotationsWebContentsObserver
    : public autofill::AutofillManager::Observer {
 public:
  UserAnnotationsWebContentsObserver(
      content::WebContents* web_contents,
      user_annotations::UserAnnotationsService* user_annotations_service);
  ~UserAnnotationsWebContentsObserver() override;

  // Creates a `UserAnnotationsWebContentsObserver` for `web_contents` if
  // allowed.
  static std::unique_ptr<UserAnnotationsWebContentsObserver>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  // autofill::AutofillManager::Observer:
  void OnFormSubmitted(autofill::AutofillManager& manager,
                       const autofill::FormData& form) override;

 private:
  // Callback invoked when AXTree for the frame has been snapshotted.
  void OnAXTreeSnapshotted(const GURL& url,
                           const std::string& title,
                           const autofill::FormData& form,
                           ui::AXTreeUpdate& snapshot);

  // The service for storing user annotations. Owned by the profile that owns
  // the web contents. Guaranteed to outlive `this`.
  const raw_ref<UserAnnotationsService> user_annotations_service_;

  // The web contents `this` is attached to. Guaranteed to outlive `this`.
  const raw_ref<content::WebContents> web_contents_;

  // Helper for observing all AutofillManagers of a WebContents.
  autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
      this};

  // Factory to create weak pointers.
  base::WeakPtrFactory<UserAnnotationsWebContentsObserver> weak_ptr_factory_{
      this};
};

}  // namespace user_annotations

#endif  // CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
