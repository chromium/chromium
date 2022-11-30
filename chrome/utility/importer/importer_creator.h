// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_IMPORTER_CREATOR_H_
#define CHROME_UTILITY_IMPORTER_IMPORTER_CREATOR_H_

#include "base/memory/ref_counted.h"
#include "chrome/common/importer/importer_type.h"

class Importer;

namespace importer {

// Creates an Importer of the specified |type|.
scoped_refptr<Importer> CreateImporterByType(ImporterType type);

}  // namespace importer

#endif  // CHROME_UTILITY_IMPORTER_IMPORTER_CREATOR_H_
