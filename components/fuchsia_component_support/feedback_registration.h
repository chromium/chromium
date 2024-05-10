// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_FEEDBACK_REGISTRATION_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_FEEDBACK_REGISTRATION_H_

#include <string_view>

namespace fuchsia_component_support {

// Overrides the default Fuchsia product info in crash reports.
// Crashes for the component |component_url| will contain |crash_product_name|,
// the version from version_info, and an appropriate value for the release
// channel. |component_url| must match the current component. The calling
// process must have access to "fuchsia.feedback.CrashReportingProductRegister".
// Registration is skipped for unofficial and unbranded builds.
void RegisterProductDataForCrashReporting(
    std::string_view absolute_component_url,
    std::string_view crash_product_name);

// Registers basic annotations for the component in |component_namespace|.
// Feedback reports will contain a namespace |component_namespace| that contains
// the version from version_info, and an appropriate value for the release
// channel. The calling process must have access to
// "fuchsia.feedback.ComponentDataRegister".
void RegisterProductDataForFeedback(std::string_view component_namespace);

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_FEEDBACK_REGISTRATION_H_
