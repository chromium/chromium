// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KCER_KCER_HISTOGRAMS_H_
#define CHROMEOS_ASH_COMPONENTS_KCER_KCER_HISTOGRAMS_H_

#include "chromeos/ash/components/kcer/kcer.h"

namespace kcer {

namespace internal {
const char KcerPkcs12ImportMetrics[] = "ChromeOS.Kcer.KcerPkcs12ImportEvent";

// Events related to import of PKCS#12 files using
// "kcer_token_impl_nss.h". These values are persisted to
// histograms. Entries should not be renumbered and numeric values should never
// be reused.
enum class KcerPkcs12ImportEvent {
  AttemptedPkcs12ChapsImport = 0,
  AttemptedPkcs12ChapsImportTask = 1,
  SuccessPkcs12ChapsImport = 2,
  AttemptedRsaKeyImportTask = 3,
  SuccessRsaKeyImportTask = 4,
  SuccessRsaCertImportTask = 5,
  AttemptedEcKeyImportTask = 6,
  SuccessEcKeyImportTask = 7,
  SuccessEcCertImportTask = 8,
  AttemptedMultipleCertImport = 9,
  SuccessMultipleCertImport = 10,
  kMaxValue = SuccessMultipleCertImport,
};

void RecordKcerPkcs12ImportUmaEvent(KcerPkcs12ImportEvent event);

}  // namespace internal

}  // namespace kcer

#endif  // CHROMEOS_ASH_COMPONENTS_KCER_KCER_HISTOGRAMS_H_
