// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_annotations/user_annotations_web_contents_observer.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/compose/buildflags.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/compose_ax_serialization_utils.h"
#endif

namespace user_annotations {

UserAnnotationsWebContentsObserver::UserAnnotationsWebContentsObserver(
    content::WebContents* web_contents,
    user_annotations::UserAnnotationsService* user_annotations_service)
    : user_annotations_service_(CHECK_DEREF(user_annotations_service)) {
  // Always ensure AutofillClientProvider is instantiated prior to observing the
  // AutofillManager. TabHelpers are currently not instantiated before
  // TabFeatures in the tab restore case. See crbug.com/362038320 for more
  // details.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(web_contents);

  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
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

  return std::make_unique<UserAnnotationsWebContentsObserver>(
      web_contents, user_annotations_service);
}

void UserAnnotationsWebContentsObserver::OnFormSubmitted(
    autofill::AutofillManager& manager,
    const autofill::FormData& form) {
  if (!user_annotations::IsUserAnnotationsObserveFormSubmissionsEnabled()) {
    return;
  }
  if (!user_annotations_service_->ShouldAddFormSubmissionForURL(form.url())) {
    return;
  }

  autofill_managers_observation_.web_contents()->RequestAXTreeSnapshot(
      base::BindOnce(&UserAnnotationsWebContentsObserver::OnAXTreeSnapshotted,
                     weak_ptr_factory_.GetWeakPtr(), form),
      ui::kAXModeWebContentsOnly,
      /*max_nodes=*/500,
      /*timeout=*/{},
      content::WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
}

void UserAnnotationsWebContentsObserver::OnAXTreeSnapshotted(
    const autofill::FormData& form,
    ui::AXTreeUpdate& snapshot) {
  optimization_guide::proto::AXTreeUpdate ax_tree;
#if BUILDFLAG(ENABLE_COMPOSE)
  ComposeAXSerializationUtils::PopulateAXTreeUpdate(snapshot, &ax_tree);
#endif
  user_annotations_service_->AddFormSubmission(std::move(ax_tree), form,
                                               base::DoNothing());
}

}  // namespace user_annotations
