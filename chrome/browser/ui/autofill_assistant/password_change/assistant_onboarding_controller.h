// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class AssistantOnboardingPrompt;

// Holds information for the consent dialog.
struct AssistantOnboardingInformation {
  AssistantOnboardingInformation();
  ~AssistantOnboardingInformation();

  AssistantOnboardingInformation(const AssistantOnboardingInformation&);
  AssistantOnboardingInformation& operator=(
      const AssistantOnboardingInformation&);

  AssistantOnboardingInformation(AssistantOnboardingInformation&&);
  AssistantOnboardingInformation& operator=(AssistantOnboardingInformation&&);

  // The resource ids of the title and the description.
  int title_id;
  int description_id;

  // The resource id of the consent text containing the legal disclaimer.
  int consent_text_id;

  // The resource id of the shown text and the URL of the "learn more" link.
  int learn_more_title_id;
  GURL learn_more_url;

  // The resource ids of the text on the buttons for declining and giving
  // consent.
  int button_cancel_text_id;
  int button_accept_text_id;
};

// Abstract interface for a controller of an `OnboardingPrompt`.
class AssistantOnboardingController {
 public:
  // A callback that is called with `true` if consent was given and false
  // otherwise (either by denying explicitly or by closing the prompt).
  // If consent was given, the resource ids of the confirmation button label
  // and other text elements are passed as arguments.
  using Callback = base::OnceCallback<
      void(bool, absl::optional<int>, const std::vector<int>&)>;

  // Factory function to create controller that is defined in the file
  // `assistant_onboarding_controller_impl.cc`.
  static std::unique_ptr<AssistantOnboardingController> Create(
      const AssistantOnboardingInformation& onboarding_information,
      content::WebContents* web_contents);

  AssistantOnboardingController() = default;
  virtual ~AssistantOnboardingController() = default;

  // Shows the `OnboardingPrompt`.
  virtual void Show(base::WeakPtr<AssistantOnboardingPrompt> prompt,
                    Callback callback) = 0;

  // Registers that the consent was given.
  virtual void OnAccept(int confirmation_grd_id,
                        const std::vector<int>& description_grd_ids) = 0;

  // Registers that the consent dialog was cancelled, i.e. no consent was given.
  virtual void OnCancel() = 0;

  // Registers that the consent prompt was closed without giving consent.
  // Depending on the type of the view, this can be due to closing a window,
  // closing a sidepanel, etc.
  virtual void OnClose() = 0;

  // Navigates to the website that contains more information about Assistant.
  virtual void OnLearnMoreClicked() = 0;

  // Provides the "model" behind the controller by returning a struct
  // specifying the consent text.
  virtual const AssistantOnboardingInformation& GetOnboardingInformation() = 0;

  // Returns a weak pointer to this controller.
  virtual base::WeakPtr<AssistantOnboardingController> GetWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_H_
