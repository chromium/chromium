// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_annotations/user_annotations_web_contents_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/compose/buildflags.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/compose_ax_serialization_utils.h"
#endif

namespace user_annotations {

UserAnnotationsWebContentsObserver::UserAnnotationsWebContentsObserver(
    content::WebContents* web_contents,
    user_annotations::UserAnnotationsService* user_annotations_service)
    : content::WebContentsObserver(web_contents),
      user_annotations_service_(user_annotations_service) {
  CHECK(user_annotations_service_);
}

UserAnnotationsWebContentsObserver::~UserAnnotationsWebContentsObserver() =
    default;

// static
std::unique_ptr<UserAnnotationsWebContentsObserver>
UserAnnotationsWebContentsObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  // Do not create an observer if the feature is disabled.
  if (!user_annotations::IsUserAnnotationsEnabled()) {
    return nullptr;
  }

  // Do not create an observer if the user annotations service is disabled for
  // this profile.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  user_annotations::UserAnnotationsService* user_annotations_service =
      UserAnnotationsServiceFactory::GetForProfile(profile);
  if (!user_annotations_service) {
    return nullptr;
  }

  return base::WrapUnique<UserAnnotationsWebContentsObserver>(
      new UserAnnotationsWebContentsObserver(web_contents,
                                             user_annotations_service));
}

void UserAnnotationsWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* rfh) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (driver) {
    driver->GetAutofillManager().AddObserver(this);
  }
}

void UserAnnotationsWebContentsObserver::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (driver) {
    driver->GetAutofillManager().RemoveObserver(this);
  }
}

void UserAnnotationsWebContentsObserver::OnFormSubmitted(
    autofill::AutofillManager& manager,
    const autofill::FormData& form) {
  if (!user_annotations::ShouldAddFormSubmissionForURL(form.url())) {
    return;
  }

  web_contents()->RequestAXTreeSnapshot(
      base::BindOnce(&UserAnnotationsWebContentsObserver::OnAXTreeSnapshotted,
                     weak_ptr_factory_.GetWeakPtr(), form),
      ui::kAXModeWebContentsOnly,
      /*max_nodes=*/500,
      /*timeout=*/{},
      content::WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
}

void UserAnnotationsWebContentsObserver::OnAXTreeSnapshotted(
    const autofill::FormData& form,
    const ui::AXTreeUpdate& snapshot) {
  optimization_guide::proto::AXTreeUpdate ax_tree;
#if BUILDFLAG(ENABLE_COMPOSE)
  ComposeAXSerializationUtils::PopulateAXTreeUpdate(snapshot, &ax_tree);
#endif
  user_annotations_service_->AddFormSubmission(ax_tree, form);
}

}  // namespace user_annotations
