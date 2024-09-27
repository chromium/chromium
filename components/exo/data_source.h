// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_SOURCE_H_
#define COMPONENTS_EXO_DATA_SOURCE_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/exo/surface.h"

namespace ui {
struct FileInfo;
enum class EndpointType;
}  // namespace ui

namespace exo {

class DataSourceDelegate;
class DataSourceObserver;
enum class DndAction;

// Object representing transferred data offered by a client.
class DataSource {
 public:
  // The maximum number of different data types that will be read by
  // GetDataForPreferredMimeTypes (plain text, RTF, HTML, image, text/uri-list,
  // application/octet-stream, chromium/x-web-custom-data).
  static constexpr int kMaxDataTypes = 7;

  explicit DataSource(DataSourceDelegate* delegate);

  DataSource(const DataSource&) = delete;
  DataSource& operator=(const DataSource&) = delete;

  ~DataSource();

  void AddObserver(DataSourceObserver* observer);
  void RemoveObserver(DataSourceObserver* observer);

  // Notifies to DataSource that the client offers new mime type.
  void Offer(const std::string& mime_type);

  // Notifies the possible drag and drop actions selected by the data source to
  // DataSource.
  void SetActions(const base::flat_set<DndAction>& dnd_actions);

  const base::flat_set<DndAction>& GetActions() const { return dnd_actions_; }

  // Notifies the data source is cancelled. e.g. Replaced with another data
  // source.
  void Cancelled();

  // Notifies the client of the mime type that will be used by the
  // recipient. Only used during drag drop operations.
  void Target(const std::optional<std::string>& mime_type);

  // Notifies the client of the dnd action that will be performed if the
  // currently running drag operation ends now. Only used during drag drop
  // operations.
  void Action(DndAction action);

  // Notifies the client that the user has released the current drag. At this
  // point the target and action are considered final, but it is still possible
  // for the recipient to reject the transfer.
  void DndDropPerformed();

  // Notifies the client that the drag was completed successfully. The data
  // source must not be used by the client after this point except to delete it.
  void DndFinished();

  // Search the set of offered MIME types for the most preferred of each of the
  // following categories: text/plain*, text/rtf, text/html*, image/*,
  // text/uri-list, chromium/x-web-custom-data. If any usable MIME types in a
  // given category are available, the corresponding
  // |*_reader| input callback will be called with the best one and the
  // corresponding data. For any category that has no available MIME types,
  // |failure_callback| is run. |failure_callback| may therefore be run as many
  // as seven times.
  using ReadDataCallback =
      base::OnceCallback<void(const std::string&, const std::vector<uint8_t>&)>;
  using ReadTextDataCallback =
      base::OnceCallback<void(const std::string&, std::u16string)>;
  using ReadFileContentsDataCallback =
      base::OnceCallback<void(const std::string&,
                              const base::FilePath&,
                              const std::vector<uint8_t>&)>;
  using ReadWebCustomDataCallback =
      base::OnceCallback<void(const std::string&, const std::vector<uint8_t>&)>;
  void ReadDataTransferEndpoint(ReadTextDataCallback dte_reader,
                                base::RepeatingClosure failure_callback);
  void GetDataForPreferredMimeTypes(
      ReadTextDataCallback text_reader,
      ReadDataCallback rtf_reader,
      ReadTextDataCallback html_reader,
      ReadDataCallback image_reader,
      ReadDataCallback filenames_reader,
      ReadFileContentsDataCallback file_contents_reader,
      ReadDataCallback web_custom_data_reader,
      base::RepeatingClosure failure_callback);

  // Read filenames and translate paths in `data` from the `source` format
  // to local paths.
  std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const;

  void ReadDataForTesting(
      const std::string& mime_type,
      ReadDataCallback callback,
      base::RepeatingClosure failure_callback = base::DoNothing());

  bool CanBeDataSourceForCopy(Surface* surface) const;

 private:
  // Reads data from the source. Then |callback| is invoked with read data. If
  // Cancelled() is invoked or DataSource is destroyed before completion,
  // |callback| is never called, and |failure_callback| is run instead.
  void ReadData(const std::string& mime_type,
                ReadDataCallback callback,
                base::OnceClosure failure_callback);

  static void OnDataRead(base::WeakPtr<DataSource> data_source_ptr,
                         ReadDataCallback callback,
                         const std::string& mime_type,
                         base::OnceClosure failure_callback,
                         const std::optional<std::vector<uint8_t>>& data);

  void OnTextRead(ReadTextDataCallback callback,
                  const std::string& mime_type,
                  const std::vector<uint8_t>& data);

  void OnFileContentsRead(ReadFileContentsDataCallback callback,
                          const std::string& mime_type,
                          const std::vector<uint8_t>& data);

  const raw_ptr<DataSourceDelegate, DanglingUntriaged> delegate_;
  base::ObserverList<DataSourceObserver> observers_;

  // Mime types which has been offered.
  std::set<std::string> mime_types_;
  bool finished_;

  base::flat_set<DndAction> dnd_actions_;

  base::WeakPtrFactory<DataSource> read_data_weak_ptr_factory_{this};
};

class ScopedDataSource {
 public:
  ScopedDataSource(DataSource* data_source, DataSourceObserver* observer);

  ScopedDataSource(const ScopedDataSource&) = delete;
  ScopedDataSource& operator=(const ScopedDataSource&) = delete;

  ~ScopedDataSource();
  DataSource* get() { return data_source_; }

 private:
  const raw_ptr<DataSource> data_source_;
  const raw_ptr<DataSourceObserver> observer_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_SOURCE_H_
