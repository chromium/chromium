// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_OMAHA_EXPERIMENT_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_OMAHA_EXPERIMENT_H_

#include <string>

#include "base/time/time.h"

namespace gcapi_internals {

extern const wchar_t kReactivationLabel[];
extern const wchar_t kRelaunchLabel[];

// Returns the full experiment label to be used by |label| (which is one of the
// labels declared above) for |brand_code|.
std::wstring GetGCAPIExperimentLabel(const wchar_t* brand_code,
                                     const std::wstring& label);

}  // namespace gcapi_internals

// The separator used to separate items in experiment labels.
extern const wchar_t kExperimentLabelSeparator;

// Writes a reactivation brand code experiment label in the Chrome product and
// binaries registry keys for |brand_code|. This experiment label will have a
// expiration date of now plus one year. If |shell_mode| is set to
// GCAPI_INVOKED_UAC_ELEVATION, the value will be written to HKLM, otherwise
// HKCU. A user cannot have both a reactivation label and a relaunch label set
// at the same time (they are mutually exclusive).
bool SetReactivationExperimentLabels(const wchar_t* brand_code, int shell_mode);

// Writes a relaunch brand code experiment label in the Chrome product and
// binaries registry keys for |brand_code|. This experiment label will have a
// expiration date of now plus one year. If |shell_mode| is set to
// GCAPI_INVOKED_UAC_ELEVATION, the value will be written to HKLM, otherwise
// HKCU. A user cannot have both a reactivation label and a relaunch label set
// at the same time (they are mutually exclusive).
bool SetRelaunchExperimentLabels(const wchar_t* brand_code, int shell_mode);

// Constructs a date string in the format understood by Google Update for the
// |current_time| plus one year.
std::wstring BuildExperimentDateString(base::Time current_time);

#endif  // CHROME_INSTALLER_GCAPI_GCAPI_OMAHA_EXPERIMENT_H_
