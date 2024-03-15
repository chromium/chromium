// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KCER_KCER_HISTOGRAMS_H_
#define CHROMEOS_COMPONENTS_KCER_KCER_HISTOGRAMS_H_

#include "base/component_export.h"
#include "chromeos/components/kcer/kcer.h"

namespace kcer {

// Events related to import of PKCS#12 files. These values are persisted to
// histograms. Entries should not be renumbered and numeric values should never
// be reused.
enum class Pkcs12MigrationUmaEvent {
  kPkcs12ImportNssSuccess = 0,
  kPkcs12ImportNssFailed = 1,
  kPkcs12ImportKcerSuccess = 2,
  kPkcs12ImportKcerFailed = 3,
  kMaxValue = kPkcs12ImportKcerFailed,
};

COMPONENT_EXPORT(KCER)
void RecordPkcs12MigrationUmaEvent(Pkcs12MigrationUmaEvent event);

COMPONENT_EXPORT(KCER)
void RecordKcerError(Error error);

}  // namespace kcer

#endif  // CHROMEOS_COMPONENTS_KCER_KCER_HISTOGRAMS_H_
