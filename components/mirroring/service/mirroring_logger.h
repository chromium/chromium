// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_LOGGER_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_LOGGER_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/mirroring/mojom/session_observer.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace mirroring {

// Logs informative and error messages for mirroring to DVLOG(1) on the console,
// and also passes them back to the SessionObserver client via IPC.
//
// Note: Pass --vmodule="mirroring_logger*=1" on the commandline to activate
// console logging.
class MirroringLogger {
 public:
  // Must be constructed on the same sequence on which `observer` is bound.
  MirroringLogger(std::string_view prefix,
                  mojo::Remote<mojom::SessionObserver>& observer);
  ~MirroringLogger();
  MirroringLogger(const MirroringLogger&) = delete;
  MirroringLogger& operator=(const MirroringLogger&) = delete;
  MirroringLogger(MirroringLogger&&) = delete;
  MirroringLogger& operator=(MirroringLogger&&) = delete;

  // Log* methods may be called on any sequence.
  void LogInfo(std::string_view message);
  void LogError(std::string_view message);
  void LogError(mojom::SessionError error, std::string_view message);

 private:
  void LogInfoInternal(const std::string& message);
  void LogErrorInternal(const std::string& message);

  const std::string prefix_;
  base::raw_ref<mojo::Remote<mojom::SessionObserver>> observer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  // Log* methods may post tasks from other sequences that outlive `this`.
  base::WeakPtrFactory<MirroringLogger> weak_ptr_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_LOGGER_H_
