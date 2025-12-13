// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_USER_EDUCATION_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_USER_EDUCATION_CONTEXT_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/interaction/framework_specific_implementation.h"

class BrowserUserEducationInterfaceImpl;

// Specialization for UserEducationContext that is tied to a Browser window.
class BrowserUserEducationContext
    : public user_education::UserEducationContext {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  BrowserUserEducationContext(
      BrowserView& browser_view,
      const user_education::UserEducationTimeProvider& time_provider);

  // Called to invalidate when the user education interface is tearing down.
  void Invalidate(base::PassKey<BrowserUserEducationInterfaceImpl>);

  // UserEducationContext:
  bool IsValid() const override;
  ui::ElementContext GetElementContext() const override;
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;

  // Retrieves the browser view. Requires that `IsValid()` is true.
  BrowserView& GetBrowserView() const;

  using PreconditionPtr =
      std::unique_ptr<user_education::FeaturePromoPrecondition>;
  using PreconditionId = user_education::FeaturePromoPrecondition::Identifier;

  // A number of preconditions are shared across all promos in the same context.
  // This returns the shared precondition with identifier `id`.
  //
  // (Since the returned value is a unique_ptr, it actually returns a thin
  // wrapper around the shared precondition implementation.)
  PreconditionPtr GetSharedPrecondition(PreconditionId id);

 protected:
  ~BrowserUserEducationContext() override;

 private:
  class ForwardingPrecondition;

  // Populates `shared_preconditions_`, but only in User Education 2.5.
  void CreateSharedPreconditions(
      const user_education::UserEducationTimeProvider& time_provider);

  raw_ptr<BrowserView> browser_view_ = nullptr;

  // Preconditions that are shared between all or a specific subset of promos
  // for this context. See `GetSharedPrecondition()`.
  std::map<PreconditionId, PreconditionPtr> shared_preconditions_;

  // Used to remotely invalidate any outstanding preconditions when this object
  // is invalidated or goes away.
  base::OnceClosureList invalidate_callbacks_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_USER_EDUCATION_CONTEXT_H_
