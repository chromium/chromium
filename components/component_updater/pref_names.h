// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_PREF_NAMES_H_
#define COMPONENTS_COMPONENT_UPDATER_PREF_NAMES_H_

#include "build/build_config.h"

namespace prefs {

extern const char kComponentUpdatesEnabled[];

extern const char kRecoveryComponentVersion[];
extern const char kRecoveryComponentUnpackPath[];

#if BUILDFLAG(IS_WIN)
// Local state prefs.
extern const char kSwReporterLastExitCode[];
extern const char kSwReporterLastTimeTriggered[];
extern const char kSwReporterLastTimeSentReport[];
extern const char kSwReporterEnabled[];
extern const char kSwReporterCohort[];
extern const char kSwReporterCohortSelectionTime[];

// Profile prefs.
extern const char kSwReporterPromptReason[];
extern const char kSwReporterPromptVersion[];
extern const char kSwReporterPromptSeed[];
extern const char kSwReporterReportingEnabled[];
#endif

}  // namespace prefs

#endif  // COMPONENTS_COMPONENT_UPDATER_PREF_NAMES_H_
