// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_DEVICE_DELEGATE_H_
#define COMPONENTS_EXO_DATA_DEVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/exo/data_offer.h"

namespace base {
class TimeTicks;
}

namespace gfx {
class PointF;
}

namespace exo {

class DataDevice;
class Surface;
enum class DndAction;

// Handles events on data devices in context-specific ways.
class DataDeviceDelegate {
 public:
  // Called at the top of the data device's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnDataDeviceDestroying(DataDevice* data_device) = 0;

  // Called when DataOffer object is delivered from a client. DataDeviceDelegate
  // has responsibility to release the returned DataOffer object.
  virtual DataOffer* OnDataOffer(DataOffer::Purpose purpose) = 0;

  // Called during a drag operation when pointer enters |surface|.
  virtual void OnEnter(Surface* surface,
                       const gfx::PointF& location,
                       const DataOffer& data_offer) = 0;

  // Called during a drag operation when pointer leaves |surface|.
  virtual void OnLeave() = 0;

  // Called during a drag operation when pointer moves on the |surface|.
  virtual void OnMotion(base::TimeTicks time_stamp,
                        const gfx::PointF& location) = 0;

  // Called during a drag operation when user drops dragging data on the
  // |surface|.
  virtual void OnDrop() = 0;

  // Called when the data is pasted on the DataDevice.
  virtual void OnSelection(const DataOffer& data_offer) = 0;

  // This should return true if |surface| is a valid target for this data
  // device. E.g. the surface is owned by the same client as the data device.
  virtual bool CanAcceptDataEventsForSurface(Surface* surface) = 0;

 protected:
  virtual ~DataDeviceDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_DEVICE_DELEGATE_H_
