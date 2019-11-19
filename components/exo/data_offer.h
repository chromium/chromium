// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_OFFER_H_
#define COMPONENTS_EXO_DATA_OFFER_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/base/class_property.h"
#include "url/gurl.h"

namespace base {
class RefCountedMemory;
}

namespace ui {
class Clipboard;
class OSExchangeData;
}

namespace exo {

class DataOfferDelegate;
class DataOfferObserver;
class FileHelper;
enum class DndAction;

// Object representing transferred data offered to a client.
class DataOffer final : public ui::PropertyHandler {
 public:
  enum Purpose {
    COPY_PASTE,
    DRAG_DROP,
  };

  DataOffer(DataOfferDelegate* delegate, Purpose purpose);
  ~DataOffer() override;

  void AddObserver(DataOfferObserver* observer);
  void RemoveObserver(DataOfferObserver* observer);

  // Notifies to the DataOffer that the client can accept |mime type|.
  void Accept(const std::string* mime_type);

  // Notifies to the DataOffer that the client start receiving data of
  // |mime_type|. DataOffer writes the request data to |fd|.
  void Receive(const std::string& mime_type, base::ScopedFD fd);

  // Notifies to the DataOffer that the client no longer uses the DataOffer
  // object.
  void Finish();

  // Notifies to the DataOffer that possible and preferred drag and drop
  // operations selected by the client.
  void SetActions(const base::flat_set<DndAction>& dnd_actions,
                  DndAction preferred_action);

  // Sets the dropped data from |data| to the DataOffer object. |file_helper|
  // will be used to convert paths to handle mount points which is mounted in
  // the mount point namespace of clinet process.
  // While this function immediately calls DataOfferDelegate::OnOffer inside it
  // with found mime types, dropped data bytes may be populated asynchronously
  // after this function call.
  // (e.g. Asynchronous lookup is required for resolving file system urls.)
  void SetDropData(FileHelper* file_helper, const ui::OSExchangeData& data);

  // Sets the clipboard data from |data| to the DataOffer object.
  void SetClipboardData(FileHelper* file_helper, const ui::Clipboard& data);

  // Sets the drag and drop actions which is offered by data source to the
  // DataOffer object.
  void SetSourceActions(const base::flat_set<DndAction>& source_actions);

  DndAction dnd_action() { return dnd_action_; }

 private:
  void OnPickledUrlsResolved(const std::string& uri_list_mime_type,
                             const std::vector<GURL>& urls);

  DataOfferDelegate* const delegate_;

  // Map between mime type and drop data bytes.
  // nullptr may be set as a temporary value until data bytes are populated.
  base::flat_map<std::string, scoped_refptr<base::RefCountedMemory>> data_;
  // Unprocessed receive requests (pairs of mime type and FD) that are waiting
  // for unpopulated (nullptr) data bytes in |data_| to be populated.
  std::vector<std::pair<std::string, base::ScopedFD>> pending_receive_requests_;

  using SendDataCallback = base::RepeatingCallback<void(base::ScopedFD)>;
  // Map from mime type (or other offered data type) to a callback that sends
  // data for that type. Using callbacks allows us to delay making copies or
  // doing other expensive processing until actually necessary.
  base::flat_map<std::string, SendDataCallback> data_callbacks_;

  base::flat_set<DndAction> source_actions_;
  DndAction dnd_action_;
  base::ObserverList<DataOfferObserver>::Unchecked observers_;
  Purpose purpose_;

  base::WeakPtrFactory<DataOffer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataOffer);
};

class ScopedDataOffer {
 public:
  ScopedDataOffer(DataOffer* data_offer, DataOfferObserver* observer);
  ~ScopedDataOffer();
  DataOffer* get() { return data_offer_; }

 private:
  DataOffer* const data_offer_;
  DataOfferObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDataOffer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_OFFER_H_
