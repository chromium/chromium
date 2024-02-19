#include "components/attribution_reporting/global_epsilon.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

// using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kGlobalEpsilon[] = "global_epsilon";

double g_max_global_epsilon = 14;

bool IsGlobalEpsilonValid(double epsilon) {
  return epsilon >= 0 && epsilon <= g_max_global_epsilon;
}

}  // namespace

// static
base::expected<GlobalEpsilon, TriggerRegistrationError>
GlobalEpsilon::Parse(const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kGlobalEpsilon);
  if (!value) {
    return GlobalEpsilon();
  }

  std::optional<double> d = value->GetIfDouble();
  if (!d.has_value()) {
    return base::unexpected(
        TriggerRegistrationError::kGlobalEpsilonWrongType);
  }

  if (!IsGlobalEpsilonValid(*d)) {
    return base::unexpected(
        TriggerRegistrationError::kGlobalEpsilonValueInvalid);
  }

  return GlobalEpsilon(*d);
}

GlobalEpsilon::GlobalEpsilon()
    : GlobalEpsilon(g_max_global_epsilon) {}

GlobalEpsilon::GlobalEpsilon(double epsilon) : epsilon_(epsilon) {
  CHECK(IsGlobalEpsilonValid(epsilon_));
}

bool GlobalEpsilon::SetIfValid(double epsilon) {
  if (!IsGlobalEpsilonValid(epsilon)) {
    return false;
  }
  epsilon_ = epsilon;
  return true;
}

void GlobalEpsilon::Serialize(base::Value::Dict& dict) const {
  dict.Set(kGlobalEpsilon, epsilon_);
}

ScopedMaxGlobalEpsilonForTesting::ScopedMaxGlobalEpsilonForTesting(
    double epsilon)
    : previous_(g_max_global_epsilon) {
  CHECK_GE(epsilon, 0);
  g_max_global_epsilon = epsilon;
}

ScopedMaxGlobalEpsilonForTesting::~ScopedMaxGlobalEpsilonForTesting() {
  g_max_global_epsilon = previous_;
}

}  // namespace attribution_reporting
