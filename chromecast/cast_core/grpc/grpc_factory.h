#ifndef THIRD_PARTY_CASTLITE_COMMON_GRPC_GRPC_FACTORY_H_
#define THIRD_PARTY_CASTLITE_COMMON_GRPC_GRPC_FACTORY_H_

#include "chromecast/cast_core/grpc/grpc_server_builder.h"

namespace cast {
namespace utils {

// This class holds a set of factory methods that should be overridden per
// specific platform (ie Android, Linux etc).
class GrpcFactory {
 public:
  // Creates an instance of GrpcServerBuilder.
  static std::unique_ptr<GrpcServerBuilder> CreateServerBuilder();
};

}  // namespace utils
}  // namespace cast

#endif  // THIRD_PARTY_CASTLITE_COMMON_GRPC_GRPC_FACTORY_H_
