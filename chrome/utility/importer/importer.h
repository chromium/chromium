// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_IMPORTER_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"

class ImporterBridge;

namespace importer {
struct SourceProfile;
}

// The base class of all importers.
class Importer : public base::RefCountedThreadSafe<Importer> {
 public:
  Importer(const Importer&) = delete;
  Importer& operator=(const Importer&) = delete;

  // All importers should implement this method by adding their import logic.
  // And it will be run in file thread by ImporterHost. Since we do async
  // import, the importer should invoke ImporterHost::NotifyImportEnded() to
  // notify its host that import stuff have been finished.
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16_t items,
                           ImporterBridge* bridge) = 0;

  // Cancels the import process.
  virtual void Cancel();

  bool cancelled() const { return cancelled_; }

 protected:
  friend class base::RefCountedThreadSafe<Importer>;

  Importer();
  virtual ~Importer();

  scoped_refptr<ImporterBridge> bridge_;

 private:
  // True if the caller cancels the import process.
  bool cancelled_;
};

#endif  // CHROME_UTILITY_IMPORTER_IMPORTER_H_
