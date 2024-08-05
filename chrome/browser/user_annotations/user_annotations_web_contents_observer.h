// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace user_annotations {

class UserAnnotationsService;

// A WebContentsObserver that listens to events that may require persisting a
// user annotation, such as a form submission, and propagates them to the
// `UserAnnotationsKeyedService` for persistence.
class UserAnnotationsWebContentsObserver
    : public content::WebContentsObserver,
      public autofill::AutofillManager::Observer {
 public:
  ~UserAnnotationsWebContentsObserver() override;

  // Creates a `UserAnnotationsWebContentsObserver` for `web_contents` if
  // allowed.
  static std::unique_ptr<UserAnnotationsWebContentsObserver>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* rfh) override;
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override;

  // autofill::AutofillManager::Observer:
  void OnFormSubmitted(autofill::AutofillManager& manager,
                       const autofill::FormData& form) override;

 private:
  UserAnnotationsWebContentsObserver(
      content::WebContents* web_contents,
      user_annotations::UserAnnotationsService* user_annotations_service);

  // Callback invoked when AXTree for the frame has been snapshotted.
  void OnAXTreeSnapshotted(const autofill::FormData& form,
                           const ui::AXTreeUpdate& snapshot);

  // The service for storing user annotations. Owned by the profile that owns
  // the web contents. Guaranteed to outlive `this`.
  raw_ptr<UserAnnotationsService> user_annotations_service_;

  // Factory to create weak pointers.
  base::WeakPtrFactory<UserAnnotationsWebContentsObserver> weak_ptr_factory_{
      this};
};

}  // namespace user_annotations

#endif  // CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
