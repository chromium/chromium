#include "chrome/common/intentive/intentive_features.h"

namespace intentive {

// Enable or disable by default as you prefer.
BASE_FEATURE(kIntentiveUI,
             "IntentiveUI",
             base::FEATURE_ENABLED_BY_DEFAULT);  // or base::FEATURE_DISABLED_BY_DEFAULT

}  // namespace intentive
