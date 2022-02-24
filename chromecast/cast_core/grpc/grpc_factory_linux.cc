#include "base/sequence_checker.h"
#include "chromecast/cast_core/grpc/grpc_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cast {
namespace utils {

namespace {

// Linux implementation of the |GrpcServerBuilder|.
class GrpcServerBuilderLinux : public GrpcServerBuilder {
 public:
  GrpcServerBuilderLinux() { server_builder_.emplace(); }
  ~GrpcServerBuilderLinux() override = default;

  // Implements GrpcServerBuilder APIs.
  GrpcServerBuilder& AddListeningPort(
      const std::string& endpoint,
      std::shared_ptr<grpc::ServerCredentials> creds,
      int* selected_port = nullptr) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(server_builder_);
    DCHECK(creds);
    server_builder_->AddListeningPort(endpoint, std::move(creds),
                                      selected_port);
    return *this;
  }

  GrpcServerBuilder& RegisterCallbackGenericService(
      grpc::CallbackGenericService* service) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(server_builder_);
    DCHECK(service);
    server_builder_->RegisterCallbackGenericService(service);
    return *this;
  }

  std::unique_ptr<grpc::Server> BuildAndStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(server_builder_) << "gRPC server was already built";
    auto server = std::move(server_builder_).value().BuildAndStart();
    server_builder_.reset();
    return server;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  absl::optional<grpc::ServerBuilder> server_builder_;
};

}  // namespace

std::unique_ptr<GrpcServerBuilder> GrpcFactory::CreateServerBuilder() {
  return std::make_unique<GrpcServerBuilderLinux>();
}

}  // namespace utils
}  // namespace cast
