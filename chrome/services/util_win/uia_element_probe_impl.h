// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_UIA_ELEMENT_PROBE_IMPL_H_
#define CHROME_SERVICES_UTIL_WIN_UIA_ELEMENT_PROBE_IMPL_H_

#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chrome/services/util_win/public/cpp/uia_element_probe_types.h"
#include "chrome/services/util_win/public/mojom/uia_element_probe.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace uia_element_probe_internal {

// Returns true if `automation_id` contains `fragment` at an ASCII
// case-insensitive word boundary (start of string, after a non-letter, or at a
// CamelCase capital, e.g. "Foo_SetDefault" and "UISetDefault" match
// "setdefault", while "ResetDefaults" and "RESETDEFAULTS" do not).
bool ContainsFragmentAtWordStart(std::wstring_view automation_id,
                                 std::wstring_view fragment);

}  // namespace uia_element_probe_internal

// Runs all UIA work on a dedicated MTA worker thread so a hung target app
// never blocks the Mojo receiver sequence or process shutdown.
class UiaElementProbeImpl : public chrome::mojom::UiaElementProbe {
 public:
  static constexpr base::TimeDelta kMaxWait = base::Seconds(30);
  static constexpr base::TimeDelta kChangeSettleDelay = base::Milliseconds(150);
  static constexpr base::TimeDelta kFallbackPollInterval = base::Seconds(1);

  explicit UiaElementProbeImpl(
      mojo::PendingReceiver<chrome::mojom::UiaElementProbe> receiver);

  UiaElementProbeImpl(const UiaElementProbeImpl&) = delete;
  UiaElementProbeImpl& operator=(const UiaElementProbeImpl&) = delete;

  ~UiaElementProbeImpl() override;

 private:
  class Worker;

  // chrome::mojom::UiaElementProbe:
  void FindElement(UiaWindowIdentity window,
                   const UiaElementQuery& query,
                   FindElementCallback callback) override;

  void OnFound(std::optional<UiaElementMeasurement> measurement);

  // The latest FindElement() call's callback, until the worker answers it.
  FindElementCallback pending_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Dedicated MTA thread with CONTINUE_ON_SHUTDOWN so hung UIA calls neither
  // block other tasks nor delay process exit, including on destruction.
  base::SequenceBound<Worker> worker_;

  // Declared after `pending_callback_` so the pipe closes before an unrun
  // callback is destroyed.
  mojo::Receiver<chrome::mojom::UiaElementProbe> receiver_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UiaElementProbeImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_SERVICES_UTIL_WIN_UIA_ELEMENT_PROBE_IMPL_H_
