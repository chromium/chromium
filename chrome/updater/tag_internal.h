// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TAG_INTERNAL_H_
#define CHROME_UPDATER_TAG_INTERNAL_H_

#include <string_view>

namespace updater::tagging {

// LINT.IfChange(TagArgs)
// The name of the bundle being installed. If not specified, the first app's
// appname is used.
constexpr inline std::string_view kTagArgBundleName = "bundlename";

// The language of the product the user is installing.
constexpr inline std::string_view kTagArgLanguage = "lang";

// Flag denoting that the user is flighting a new test feature.
constexpr inline std::string_view kTagArgFlighting = "flighting";

// Flag denoting that the user has agreed to provide usage stats, crashreports
// etc.
constexpr inline std::string_view kTagArgUsageStats = "usagestats";

// A unique value for this installation session. It can be used to follow the
// progress from the website to installation completion.
constexpr inline std::string_view kTagArgInstallationId = "iid";

// The brand code used for branding. This value sets the initial brand for the
// updater and the client app. If a brand value already exists on the system,
// the new brand value is ignored.
constexpr inline std::string_view kTagArgBrandCode = "brand";

// The Client ID used for branding.
// If a client value already exists on the system, it should be ignored.
// This value is used to set the initial client for the updater and the client
// app.
constexpr inline std::string_view kTagArgClientId = "client";

// A set of experiment labels used to track installs that are included in
// experiments. Use "experiments" for per-app arguments; use "omahaexperiments"
// for updater-specific labels.
constexpr inline std::string_view kAppArgExperimentLabels = "experiments";
constexpr inline std::string_view kTagArgOmahaExperimentLabels =
    "omahaexperiments";

// A referral ID used for tracking referrals.
constexpr inline std::string_view kTagArgReferralId = "referral";

// Tells the updater what ap value to set in the registry.
constexpr inline std::string_view kAppArgAdditionalParameters = "ap";

// Indicates which browser to restart on successful install.
constexpr inline std::string_view kTagArgBrowserType = "browser";

// Runtime Mode: "runtime" argument in the tag tells the updater to install
// itself and stay on the system without any associated application for at least
// `kMaxServerStartsBeforeFirstReg` wakes. This feature is used to expose the
// COM API to a process that will install applications via that API.
// Example:
//   "runtime=true&needsadmin=true"
constexpr inline std::string_view kTagArgRuntimeMode = "runtime";

// Enrollment token: "etoken" argument in the tag tells the per-machine updater
// to register the machine to the device management server. The value must be a
// GUID.
// Example:
//   "etoken=5d086552-4514-4dfb-8a3e-337024ec35ac"
constexpr inline std::string_view kTagArgEnrollmentToken = "etoken";

// The list of arguments that are needed for a meta-installer, to
// indicate which application is being installed. These are stamped
// inside the meta-installer binary.
constexpr inline std::string_view kTagArgAppId = "appguid";
constexpr inline std::string_view kAppArgAppName = "appname";
constexpr inline std::string_view kTagArgNeedsAdmin = "needsadmin";
constexpr inline std::string_view kAppArgInstallDataIndex = "installdataindex";
constexpr inline std::string_view kAppArgUntrustedData = "untrusteddata";

// This switch allows extra data to be communicated to the application
// installer. The extra data needs to be URL-encoded. The data will be decoded
// and written to the file, that is then passed in the command line to the
// application installer in the form "/installerdata=blah.dat". One per
// application.
constexpr inline std::string_view kAppArgInstallerData = "installerdata";
// LINT.ThenChange(tag_fuzztest.cc:TagArgs)

}  // namespace updater::tagging

#endif  // CHROME_UPDATER_TAG_INTERNAL_H_
