// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_SOURCE_H_
#define COMPONENTS_EXO_DATA_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/exo/surface.h"

namespace exo {

class DataSourceDelegate;
class DataSourceObserver;
enum class DndAction;

// Object representing transferred data offered by a client.
class DataSource {
 public:
  explicit DataSource(DataSourceDelegate* delegate);
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
  void Target(const base::Optional<std::string>& mime_type);

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
  // following categories: text/plain*, text/rtf, text/html*, image/*. If any
  // usable MIME types in a given category are available, the corresponding
  // |*_reader| input callback will be called with the best one and the
  // corresponding data. For any category that has no available MIME types,
  // |failure_callback| is run. |failure_callback| may therefore be run as many
  // as four times.
  using ReadDataCallback =
      base::OnceCallback<void(const std::string&, const std::vector<uint8_t>&)>;
  using ReadTextDataCallback =
      base::OnceCallback<void(const std::string&, base::string16)>;
  void GetDataForPreferredMimeTypes(ReadTextDataCallback text_reader,
                                    ReadDataCallback rtf_reader,
                                    ReadTextDataCallback html_reader,
                                    ReadDataCallback image_reader,
                                    base::RepeatingClosure failure_callback);

  void ReadDataForTesting(const std::string& mime_type,
                          ReadDataCallback callback);

  bool CanBeDataSourceForCopy(Surface* surface) const;

 private:
  // Reads data from the source. Then |callback| is invoked with read data. If
  // Cancelled() is invoked or DataSource is destroyed before completion,
  // |callback| is never called, and |failure_callback| is run instead.
  void ReadData(const std::string& mime_type,
                ReadDataCallback callback,
                base::OnceClosure failure_callback);

  void OnDataRead(ReadDataCallback callback,
                  const std::string& mime_type,
                  base::OnceClosure failure_callback,
                  const base::Optional<std::vector<uint8_t>>& data);

  void OnTextRead(ReadTextDataCallback callback,
                  const std::string& mime_type,
                  const std::vector<uint8_t>& data);

  DataSourceDelegate* const delegate_;
  base::ObserverList<DataSourceObserver>::Unchecked observers_;

  // Mime types which has been offered.
  std::set<std::string> mime_types_;
  bool finished_;

  base::flat_set<DndAction> dnd_actions_;

  base::WeakPtrFactory<DataSource> read_data_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

class ScopedDataSource {
 public:
  ScopedDataSource(DataSource* data_source, DataSourceObserver* observer);
  ~ScopedDataSource();
  DataSource* get() { return data_source_; }

 private:
  DataSource* const data_source_;
  DataSourceObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDataSource);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_SOURCE_H_
