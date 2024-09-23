#ifndef CHROMECAST_CAST_CORE_GRPC_TEST_UTILS_H_
#define CHROMECAST_CAST_CORE_GRPC_TEST_UTILS_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromecast/cast_core/grpc/grpc_server.h"

namespace cast {
namespace test {

// Stops the gRPC server in a separate task runner to avoid blocking the main
// test thread, and keep latter's run loop spinning.  The process will crash
// in case the |timeout| is reached as such case clearly points to a bug in
// reactor handling.
void StopGrpcServer(utils::GrpcServer& server, const base::TimeDelta& timeout);

// Waits for |timeout| time for predicate to return true.
bool WaitForPredicate(const base::TimeDelta& timeout,
                      base::RepeatingCallback<bool()> predicate);

}  // namespace test
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_TEST_UTILS_H_
