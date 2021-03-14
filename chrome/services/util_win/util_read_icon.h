// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_UTIL_READ_ICON_H_
#define CHROME_SERVICES_UTIL_WIN_UTIL_READ_ICON_H_

#include <string>

#include "base/macros.h"
#include "chrome/services/util_win/public/mojom/util_read_icon.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class UtilReadIcon : public chrome::mojom::UtilReadIcon {
 public:
  explicit UtilReadIcon(
      mojo::PendingReceiver<chrome::mojom::UtilReadIcon> receiver);
  ~UtilReadIcon() override;

 private:
  // chrome::mojom::UtilReadIcon:
  void ReadIcon(const base::FilePath& filename,
                chrome::mojom::IconSize icon_size,
                ReadIconCallback callback) override;

  mojo::Receiver<chrome::mojom::UtilReadIcon> receiver_;

  base::WeakPtrFactory<UtilReadIcon> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(UtilReadIcon);
};

#endif  // CHROME_SERVICES_UTIL_WIN_UTIL_READ_ICON_H_
