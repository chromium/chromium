// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_client_ios.h"

#import "base/containers/flat_set.h"
#import "base/no_destructor.h"
#import "ios/web/public/web_state_user_data.h"

namespace autofill {
namespace {

// Helper class that allow attaching an AutofillClientIOS with a WebState.
//
// This is a temporary solution to progressively migrate from to a state where
// AutofillClientIOS is a WebStateUserData. See https://crbug.com/441444002 for
// details.
class AutofillClientIOSHandle
    : public web::WebStateUserData<AutofillClientIOSHandle> {
 public:
  ~AutofillClientIOSHandle() override {}

  AutofillClientIOS* client() { return client_; }

 private:
  friend class web::WebStateUserData<AutofillClientIOSHandle>;
  AutofillClientIOSHandle(web::WebState* web_state, AutofillClientIOS* client)
      : client_(client) {}

  raw_ptr<AutofillClientIOS, DanglingUntriaged> client_;
};

}  // namespace

// static
AutofillClientIOS* AutofillClientIOS::FromWebState(web::WebState* web_state) {
  if (auto* handle = AutofillClientIOSHandle::FromWebState(web_state)) {
    return handle->client();
  }

  return nullptr;
}

AutofillClientIOS::AutofillClientIOS(web::WebState* web_state,
                                     id<AutofillDriverIOSBridge> bridge)
    : web_state_(web_state->GetWeakPtr()),
      autofill_driver_factory_(this, bridge) {
  CHECK(!FromWebState(web_state_.get()));
  AutofillClientIOSHandle::CreateForWebState(web_state, this);
}

AutofillClientIOS::~AutofillClientIOS() {
  // Some instances may outlive the WebState, so unregister if needed.
  if (web::WebState* web_state = web_state_.get()) {
    AutofillClientIOSHandle::RemoveFromWebState(web_state);
  }
}

AutofillDriverIOSFactory& AutofillClientIOS::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

}  // namespace autofill
