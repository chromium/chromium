// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/test_autofill_client_injector.h"

#include "base/check.h"
#include "base/check_op.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

size_t TestAutofillClientInjectorBase::num_instances_ = 0;

// static
bool TestAutofillClientInjectorBase::ShouldInject(
    content::WebContents* web_contents) {
  // Autofill clients need keyed services, which browser contexts that disable
  // them by default don't have. Such contexts never get autofill clients in
  // production.
  return !BrowserContextDependencyManager::GetInstance()
              ->AreBrowserContextServicesDisabledByDefault(
                  web_contents->GetBrowserContext());
}

TestAutofillClientInjectorBase::TestAutofillClientInjectorBase() {
  CHECK(!some_instance_is_alive())
      << "At most one instance is allowed per TestAutofillClientInjectors";
  CHECK(!TestAutofillDriverInjectorBase::some_instance_is_alive())
      << "TestAutofillClientInjector must be created before any "
         "TestAutofillDriverInjector";
  CHECK(!TestAutofillManagerInjectorBase::some_instance_is_alive())
      << "TestAutofillClientInjector must be created before any "
         "TestAutofillManagerInjector";
  ++num_instances_;
}

TestAutofillClientInjectorBase::~TestAutofillClientInjectorBase() {
  DCHECK_GE(num_instances_, 1u);
  --num_instances_;
}

}  // namespace autofill
