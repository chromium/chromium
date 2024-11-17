#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SWITCHES_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SWITCHES_H_

#include <optional>

#include "components/optimization_guide/proto/features/forms_annotations.pb.h"

namespace user_annotations {
namespace switches {

extern const char kFormsAnnotationsOverride[];

std::optional<optimization_guide::proto::FormsAnnotationsResponse>
ParseFormsAnnotationsFromCommandLine();

}  // namespace switches
}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SWITCHES_H_
