// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer.h"

#include "chrome/common/importer/importer_bridge.h"

void Importer::Cancel() {
  cancelled_ = true;
}

Importer::Importer() : cancelled_(false) {}

Importer::~Importer() {}
