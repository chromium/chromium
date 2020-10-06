// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_

#include "chromecast/bindings/bindings_manager.h"
#include "chromecast/browser/cast_web_contents.h"
#include "components/cast/api_bindings/manager.h"

namespace chromecast {
namespace bindings {

class NamedMessagePortConnectorCast;

// Implements the CastOS BindingsManager.
class BindingsManagerCast : public BindingsManager,
                            public CastWebContents::Observer {
 public:
  explicit BindingsManagerCast(chromecast::CastWebContents* cast_web_contents);
  ~BindingsManagerCast() override;

  BindingsManagerCast(const BindingsManagerCast&) = delete;
  void operator=(const BindingsManagerCast&) = delete;

  // BindingsManager implementation.
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

 private:
  // CastWebContents::Observer implementation.
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;

  chromecast::CastWebContents* cast_web_contents_;
  std::unique_ptr<NamedMessagePortConnectorCast> port_connector_;
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
