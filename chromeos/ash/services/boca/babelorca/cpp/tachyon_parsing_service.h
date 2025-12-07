// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BOCA_BABELORCA_CPP_TACHYON_PARSING_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_BOCA_BABELORCA_CPP_TACHYON_PARSING_SERVICE_H_

#include <memory>
#include <string>

#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::babelorca {

class ProtoHttpStreamParser;

class TachyonParsingService : public mojom::TachyonParsingService {
 public:
  explicit TachyonParsingService(
      mojo::PendingReceiver<mojom::TachyonParsingService> receiver);

  TachyonParsingService(const TachyonParsingService&) = delete;
  TachyonParsingService& operator=(const TachyonParsingService&) = delete;

  ~TachyonParsingService() override;

 private:
  // mojom::TachyonParsingService:
  void Parse(const std::string& stream_data, ParseCallback callback) override;

  mojo::Receiver<mojom::TachyonParsingService> receiver_;

  std::unique_ptr<ProtoHttpStreamParser> stream_parser_;

  mojom::ParsingState parsing_state_ = mojom::ParsingState::kOk;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_SERVICES_BOCA_BABELORCA_CPP_TACHYON_PARSING_SERVICE_H_
