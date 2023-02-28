// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_FLUSHER_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_FLUSHER_H_

#include <memory>

#include "base/component_export.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class FileFlusher;

// Manages flushing the files under browser_context's directory.
// Destroying of this instance will cancel the pending flushes if any
// asynchronously at earliest timing. It will not block the thread where
// this is being destroyed.
class COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER) BrowserContextFlusher {
 public:
  BrowserContextFlusher();
  BrowserContextFlusher(const BrowserContextFlusher&) = delete;
  BrowserContextFlusher& operator=(const BrowserContextFlusher&) = delete;
  ~BrowserContextFlusher();

  // Returns the global instance. BrowserContextFlusher is effectively a
  // singleton in a product process.
  static BrowserContextFlusher* Get();

  // Schedules to flush the files under the |browser_context| directory.
  // Actual flush will run in a background task runner.
  void ScheduleFlush(content::BrowserContext* browser_context);

 private:
  std::unique_ptr<FileFlusher> flusher_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_FLUSHER_H_
