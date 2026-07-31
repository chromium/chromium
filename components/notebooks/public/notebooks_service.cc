// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/public/notebooks_service.h"

namespace notebooks {

NotebooksService::Observer::~Observer() = default;

void NotebooksService::Observer::OnNotebooksModelLoaded() {}
void NotebooksService::Observer::OnNotebookAdded(const Notebook& notebook) {}
void NotebooksService::Observer::OnNotebookUpdated(const Notebook& notebook) {}
void NotebooksService::Observer::OnNotebookRemoved(const NotebookId& id) {}

NotebooksService::NotebooksService() = default;
NotebooksService::~NotebooksService() = default;

}  // namespace notebooks
