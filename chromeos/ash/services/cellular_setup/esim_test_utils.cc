// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/esim_test_utils.h"

#include "base/run_loop.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"

namespace ash::cellular_setup {

mojom::EuiccPropertiesPtr GetEuiccProperties(
    const mojo::Remote<mojom::Euicc>& euicc) {
  mojom::EuiccPropertiesPtr result;
  base::RunLoop run_loop;
  euicc->GetProperties(base::BindOnce(
      [](mojom::EuiccPropertiesPtr* out, base::OnceClosure quit_closure,
         mojom::EuiccPropertiesPtr properties) {
        *out = std::move(properties);
        std::move(quit_closure).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

mojom::ESimProfilePropertiesPtr GetESimProfileProperties(
    const mojo::Remote<mojom::ESimProfile>& esim_profile) {
  mojom::ESimProfilePropertiesPtr result;
  base::RunLoop run_loop;
  esim_profile->GetProperties(base::BindOnce(
      [](mojom::ESimProfilePropertiesPtr* out, base::OnceClosure quit_closure,
         mojom::ESimProfilePropertiesPtr properties) {
        *out = std::move(properties);
        std::move(quit_closure).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

std::vector<mojo::PendingRemote<mojom::ESimProfile>> GetProfileList(
    const mojo::Remote<mojom::Euicc>& euicc) {
  std::vector<mojo::PendingRemote<mojom::ESimProfile>> result;
  base::RunLoop run_loop;
  euicc->GetProfileList(base::BindOnce(
      [](std::vector<mojo::PendingRemote<mojom::ESimProfile>>* out,
         base::OnceClosure quit_closure,
         std::vector<mojo::PendingRemote<mojom::ESimProfile>> profile_list) {
        *out = std::move(profile_list);
        std::move(quit_closure).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

}  // namespace ash::cellular_setup
