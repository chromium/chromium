// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/test_autofill_client_ios.h"

#import "base/containers/flat_map.h"
#import "base/no_destructor.h"
#import "ios/web/public/web_state.h"

namespace autofill {

namespace {
base::flat_map<web::WebState*, AutofillClientIOS*>& GetDriverRegistry() {
  static base::NoDestructor<base::flat_map<web::WebState*, AutofillClientIOS*>>
      g_web_state_to_driver;
  return *g_web_state_to_driver;
}
}  // namespace

AutofillClientIOS* FakeFromWebState(web::WebState* web_state) {
  return GetDriverRegistry()[web_state];
}

void AddToFakeWebStateRegistry(AutofillClientIOS* client) {
  GetDriverRegistry()[client->web_state()] = client;
}

void RemoveFromFakeWebStateRegistry(AutofillClientIOS* client) {
  GetDriverRegistry()[client->web_state()] = nullptr;
}

}  // namespace autofill
