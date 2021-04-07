// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/history/core/browser/history_types.h"

namespace sql {
class Database;
}  // namespace sql

namespace history {

// Holds annotations made for a user's visits.
class VisitAnnotationsDatabase {
 public:
  // Must call InitAnnotationsTables before using any other part of this class.
  VisitAnnotationsDatabase();
  VisitAnnotationsDatabase(const VisitAnnotationsDatabase&) = delete;
  VisitAnnotationsDatabase& operator=(const VisitAnnotationsDatabase&) = delete;
  virtual ~VisitAnnotationsDatabase();

  // Adds a line to the content annotations database with the given information,
  // returning true on success, false on failure. The given content annotations
  // are updated with the new row on success.
  bool AddContentAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentAnnotations& visit_content_annotations);

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The content annotations for the visit must exist.
  // Returns true on success.
  bool UpdateContentAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentAnnotations& visit_content_annotations);

  // Query for a VisitContentAnnotations given a visit id and returns it if
  // present. If it's present, `out_content_annotations` will be filled with the
  // expected data; otherwise, `out_content_annotations` won't be changed.
  bool GetContentAnnotationsForVisit(
      VisitID visit_id,
      VisitContentAnnotations* out_content_annotations);
  // Deletes content annotations currently using the provided visit for
  // representation. This will also delete any associated annotations usage
  // data. If no content annotations exist for the visit id, this is a no-op.
  bool DeleteContentAnnotationsForVisit(VisitID visit_id);

  // Called by the derived classes to migrate the older visits table's
  // floc_allowed (for historical reasons named "publicly_routable" in the
  // schema) column to the content_annotations table, from a BOOLEAN filed to
  // a bit masking INTEGER filed.
  bool MigrateFlocAllowedToAnnotationsTable();

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Creates the tables used by this class if necessary. Returns true on
  // success.
  bool InitVisitAnnotationsTables();

  // Deletes all the annotations tables, returning true on success.
  bool DropVisitAnnotationsTables();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
