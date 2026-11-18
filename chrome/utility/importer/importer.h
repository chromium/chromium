// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_IMPORTER_H_

#include <stdint.h>

#include <atomic>
#include <memory>

#include "base/memory/ref_counted.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class ImporterBridge;

namespace user_data_importer {
struct SourceProfile;
}  // namespace user_data_importer

// The base class of all importers.
class Importer : public base::RefCountedThreadSafe<Importer> {
 public:
  Importer(const Importer&) = delete;
  Importer& operator=(const Importer&) = delete;

  // All importers should implement this method by adding their import logic.
  // And it will be run in file thread by ImporterHost. Since we do async
  // import, the importer should invoke ImporterHost::NotifyImportEnded() to
  // notify its host that import stuff have been finished.
  virtual void StartImport(
      const user_data_importer::SourceProfile& source_profile,
      uint16_t items,
      ImporterBridge* bridge) = 0;

  // Provides a remote to a sandboxed BookmarkHtmlParser running in another
  // process. Must be called before StartImport().
  void SetBookmarkHtmlParser(
      mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser>
          parser);

  // Cancels the import process.
  virtual void Cancel();

  bool cancelled() const { return cancelled_.load(std::memory_order_relaxed); }

 protected:
  friend class base::RefCountedThreadSafe<Importer>;

  Importer();
  virtual ~Importer();

  // Binds and returns the BookmarkHtmlParser remote on the import thread.
  // Callers must keep ownership of the bound Remote on the import thread (e.g.
  // in Mojo callbacks) rather than storing it on `Importer`, which may be
  // destroyed on the main utility thread.
  std::unique_ptr<mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>
  BindBookmarkHtmlParser();

  scoped_refptr<ImporterBridge> bridge_;

 private:
  mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser>
      bookmark_html_parser_remote_;

  // True if the caller cancels the import process.
  std::atomic<bool> cancelled_{false};
};

#endif  // CHROME_UTILITY_IMPORTER_IMPORTER_H_
