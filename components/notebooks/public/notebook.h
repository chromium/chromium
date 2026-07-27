// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOK_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOK_H_

#include "base/time/time.h"
#include "components/notebooks/public/notebook_id.h"

namespace notebooks {

// Represents a Notebook entity in Chromium.
//
// This class serves as the core in-memory domain model for Notebooks on the
// client device.
class Notebook {
 public:
  // Constructs a Notebook with its required fields.
  Notebook(NotebookId id, base::Time creation_time, base::Time update_time);

  Notebook(const Notebook&);
  Notebook& operator=(const Notebook&);
  Notebook(Notebook&&);
  Notebook& operator=(Notebook&&);
  ~Notebook();

  // Metadata accessors.
  const NotebookId& id() const { return id_; }
  base::Time creation_time() const { return creation_time_; }
  base::Time update_time() const { return update_time_; }

  // Metadata mutators.
  Notebook& SetUpdateTime(base::Time update_time);

  // Returns true if all syncable fields in `this` and `other` are equal.
  friend bool operator==(const Notebook&, const Notebook&) = default;

 private:
  // Unique client-side identifier for this notebook.
  NotebookId id_;

  // Timestamp when the notebook was created on client.
  base::Time creation_time_;

  // Timestamp when the notebook was last updated.
  base::Time update_time_;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOK_H_
