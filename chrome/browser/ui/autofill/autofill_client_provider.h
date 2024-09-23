// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_PROVIDER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_PROVIDER_H_

#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace content {
class WebContents;
}

namespace autofill {

// This serves as a factory for Desktop and Android. It instantiates the correct
// `ContentAutofillClient` for each `WebContents` instance.
// The created client can depend on platform-specific prefs and features but is
// always of the same type across all WebContents instances.
class AutofillClientProvider : public KeyedService {
 public:
  explicit AutofillClientProvider(PrefService* prefs);
  AutofillClientProvider(const AutofillClientProvider&) = delete;
  AutofillClientProvider& operator=(const AutofillClientProvider&) = delete;
  ~AutofillClientProvider() override;

  // For the given `web_contents`, creates either a new
  //  * ChromeAutofillClient if Chrome provides its own Autofill services, or
  //  * AndroidAutofillClient if Chrome delegate autofilling to Android.
  // It's a no-op if any ContentAutofillClient is already associated with the
  // given `web_contents`.
  void CreateClientForWebContents(content::WebContents* web_contents);

  // The return value is constant once this provider has been created. The
  // method returns true iff platform autofill should be used instead of
  // built-in autofill.
  bool uses_platform_autofill() const { return uses_platform_autofill_; }

 private:
  const bool uses_platform_autofill_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_PROVIDER_H_
