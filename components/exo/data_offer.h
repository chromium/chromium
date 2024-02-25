// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_OFFER_H_
#define COMPONENTS_EXO_DATA_OFFER_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/base/class_property.h"
#include "url/gurl.h"

namespace aura {
class Window;
}

namespace base {
class Pickle;
class RefCountedMemory;
}

namespace ui {
class Clipboard;
class OSExchangeData;
enum class EndpointType;
}

namespace exo {

class DataOfferDelegate;
class DataOfferObserver;
class DataExchangeDelegate;
enum class DndAction;

// Object representing transferred data offered to a client.
class DataOffer final : public ui::PropertyHandler {
 public:
  using SendDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  using AsyncSendDataCallback = base::OnceCallback<void(SendDataCallback)>;

  DataOffer(DataOfferDelegate* delegate);

  DataOffer(const DataOffer&) = delete;
  DataOffer& operator=(const DataOffer&) = delete;

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

  // Sets the dropped data from |data| to the DataOffer object.
  // |data_exchange_delegate| will be used to convert paths to handle mount
  // points which is mounted in the mount point namespace of client process.
  // |target| is the drop target window and can be used to apply the target
  // specitic logic to interpret the data. While this function immediately calls
  // DataOfferDelegate::OnOffer inside it with found mime types, dropped data
  // bytes may be populated asynchronously after this function call. (e.g.
  // Asynchronous lookup is required for resolving file system urls.)
  void SetDropData(DataExchangeDelegate* data_exchange_delegate,
                   aura::Window* target,
                   const ui::OSExchangeData& data);

  // Sets the clipboard data from |data| to the DataOffer object.
  void SetClipboardData(DataExchangeDelegate* data_exchange_delegate,
                        const ui::Clipboard& data,
                        ui::EndpointType endpoint_type);

  // Sets the drag and drop actions which is offered by data source to the
  // DataOffer object.
  void SetSourceActions(const base::flat_set<DndAction>& source_actions);

  DndAction dnd_action() const { return dnd_action_; }
  bool finished() const { return finished_; }

 private:
  void OnDataReady(const std::string& mime_type,
                   base::ScopedFD fd,
                   scoped_refptr<base::RefCountedMemory> data);
  void GetUrlsFromPickle(DataExchangeDelegate* data_exchange_delegate,
                         aura::Window* target,
                         const base::Pickle& pickle,
                         SendDataCallback callback);
  void OnPickledUrlsResolved(SendDataCallback callback,
                             const std::vector<GURL>& urls);

  const raw_ptr<DataOfferDelegate, DanglingUntriaged> delegate_;

  // Data for a given mime type may not ever be requested, or may be requested
  // more than once. Using callbacks and a cache allows us to delay any
  // expensive operations until they are required, and then ensure that they are
  // performed at most once. When we offer data for a given mime type we will
  // populate |data_callbacks_| with mime type and a callback which will produce
  // the required data. On the first request to |Receive()| we remove and invoke
  // the callback and set |data_cache_| with null data. When the callback
  // completes we populate |data_cache_| with data and fulfill any
  // |pending_receive_requests|.
  base::flat_map<std::string, AsyncSendDataCallback> data_callbacks_;
  base::flat_map<std::string, scoped_refptr<base::RefCountedMemory>>
      data_cache_;
  std::vector<std::pair<std::string, base::ScopedFD>> pending_receive_requests_;

  base::flat_set<DndAction> source_actions_;
  DndAction dnd_action_;
  base::ObserverList<DataOfferObserver>::Unchecked observers_;
  bool finished_;

  base::WeakPtrFactory<DataOffer> weak_ptr_factory_{this};
};

class ScopedDataOffer {
 public:
  ScopedDataOffer(DataOffer* data_offer, DataOfferObserver* observer);

  ScopedDataOffer(const ScopedDataOffer&) = delete;
  ScopedDataOffer& operator=(const ScopedDataOffer&) = delete;

  ~ScopedDataOffer();
  DataOffer* get() { return data_offer_; }

 private:
  const raw_ptr<DataOffer> data_offer_;
  const raw_ptr<DataOfferObserver> observer_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_OFFER_H_
