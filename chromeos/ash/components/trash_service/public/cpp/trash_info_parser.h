// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_PUBLIC_CPP_TRASH_INFO_PARSER_H_
#define CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_PUBLIC_CPP_TRASH_INFO_PARSER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::trash_service {

// A class that manages the lifetime and mojo connection of the Trash service.
class TrashInfoParser {
 public:
  TrashInfoParser();
  ~TrashInfoParser();

  TrashInfoParser(const TrashInfoParser&) = delete;
  TrashInfoParser& operator=(const TrashInfoParser&) = delete;

  void ParseTrashInfoFile(const base::FilePath& path,
                          ParseTrashInfoCallback callback);

  void SetDisconnectHandler(base::OnceCallback<void()> disconnect_callback);

 private:
  void OnGotFile(ParseTrashInfoCallback callback, base::File file);

  // A connection to the underlying TrashService.
  mojo::Remote<mojom::TrashService> service_;
  base::WeakPtrFactory<TrashInfoParser> weak_ptr_factory_{this};
};

}  // namespace ash::trash_service

#endif  // CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_PUBLIC_CPP_TRASH_INFO_PARSER_H_
