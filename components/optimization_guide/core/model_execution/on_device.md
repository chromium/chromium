# On-Device Model Execution overview

This directory defines the OnDeviceCapability API, it's implementations,
and utility objects and methods for working with it.  This API allows the
use of shared on-device models, and the code here manages download of assets
and instantiation of shared model resources.

There are 4 major participants in the logic here:

1. The Client that wants to use the model. This directory also provides
wrapper objects that act on behalf of the Client. This code may run in
any process.

2. The Broker, which the client initiates usage with. This code runs in the
browser process, and there are different implementations on different platforms.

3. The AssetProviders, which the broker uses to download models and configs.
Different implementations use different providers.

4. The Service, which the broker direct to load models into memory.

```mermaid
sequenceDiagram
    actor Client
    Client-)Broker: RequestAssets
    Broker-)AssetProvider: Register
    activate AssetProvider
    Client-)Broker: Subscribe
    AssetProvider-->>Broker: Assets
    deactivate AssetProvider
    Broker--)Client: Solution
    Client-)Broker: CreateSession
    Broker-)Service: LoadModel
    activate Service
    Broker-)Service: LoadAdaptation
    Broker-)Service: CreateSession
    Broker--)Client: Session
    Client-)Service: Append
    Client-)Service: Generate
    Service--)Client: Output
    deactivate Service
```

# Desktop Broker

The desktop implementation use the on_device_model service utility process (see
//services/on_device_model) as it's Service and gets Assets from both
component updater and an optimization_guide::ModelProvider.  The main
implementation class is `ModelBrokerState`, which composes several parts.
This is instantiated as part of a GlobalFeature in the browser process.

Note: the diagram below shows how the classes *should* be organized, but they
don't quite match this yet. Notably OnDeviceAssetManager is different.

```mermaid
classDiagram
    class OnDeviceCapability {
        <<interface>>
    }
    class ModelBrokerState {
        Composes broker logic
    }
    class PerformanceClassifier {
        Determines device capabilities
    }
    class UsageTracker {
        Tracks which features requested
    }
    class OnDeviceAssetManager {
        Downloads required assets
    }
    class OnDeviceModelServiceController {
        Loads models in the service process on-demand
    }
    class ModelBrokerImpl {
        Broadcasts Solution updates
    }
    class OnDeviceModelComponentStateManager {
        Downloads foundational model
    }
    class SupplementalModelLoader {
        Downloads safety/language models
    }
    class AdaptationLoaderMap {
        Downloads feature configs (and LORAs)
    }
    OnDeviceCapability <|-- ModelBrokerState
    ModelBrokerState *-- UsageTracker
    ModelBrokerState *-- PerformanceClassifier
    ModelBrokerState *-- OnDeviceAssetManager
    ModelBrokerState *-- OnDeviceModelServiceController
    ModelBrokerState *-- ModelBrokerImpl

    OnDeviceAssetManager ..> PerformanceClassifier : reads from
    OnDeviceAssetManager ..> UsageTracker : observes for triggers
    OnDeviceAssetManager *-- OnDeviceModelComponentStateManager
    OnDeviceAssetManager *-- SupplementalModelLoader
    OnDeviceAssetManager *-- AdaptationLoaderMap

    OnDeviceAssetManager ..> OnDeviceModelServiceController : provides assets
    OnDeviceModelServiceController *-- ModelControllers
    OnDeviceModelServiceController *-- SafetyClient
    OnDeviceModelServiceController ..> ModelBrokerImpl : provides Solutions
```

# Android Broker

The android implementation uses AICore as the Service, and as the AssetProvider.
It downloads additional assets from optimization_guide::ModelProvider.
The main class is ModelBrokerAndroid, which is analogous to ModelBrokerState.
Chrome will download and own feature configs that match the models found in AICore.

Android:
```mermaid
classDiagram
    OnDeviceCapability <|-- ModelBrokerAndroid
    ModelBrokerAndroid *-- UsageTracker
    ModelBrokerAndroid *-- ModelBrokerImpl
    ModelBrokerAndroid *-- SolutionFactory
    SolutionFactory *-- ModelDownloaderAndroid
    SolutionFactory *-- AdaptationLoaderMap
```

# Client

Several objects are provided for working with optimization_guide::mojom::ModelBroker
and on_device_model::mojom::Sessions mojo objects.  These objects act in the
"Client" space and generally provide state management and config-driven behaviors.

Client:
```mermaid
classDiagram
    class ModelBrokerClient {
        Wraps mojom::ModelBroker
    }
    class OnDeviceSession {
        <<interface>>
    }
    class SessionImpl {
        Feature config driven wrapper for mojom::Session
    }
    class OnDeviceExecution {
        Tracks state for work triggered by an Execute call.
    }
    class OnDeviceModelFeatureAdapter {
        A parsed feature config
    }
    class ResponseParser {
        <<interface>>
    }
    class SafetyChecker {
        Config driven wrapper for mojom::TextSafetySession
    }
    class SafetyConfig {
        A parsed TextSafetyConfig proto
    }

    ModelBrokerClient *-- ModelSubscriber
    ModelSubscriber *-- ModelClient
    ModelClient ..> OnDeviceSession : creates
    OnDeviceSession <|-- SessionImpl
    SessionImpl *-- OnDeviceContext
    SessionImpl *-- OnDeviceExecution
    OnDeviceContext *-- OnDeviceOptions
    OnDeviceExecution *-- OnDeviceOptions
    OnDeviceOptions *-- OnDeviceModelFeatureAdapter
    OnDeviceOptions *-- SafetyChecker
    OnDeviceModelFeatureAdapter *-- ResponseParser
    SafetyChecker *-- SafetyConfig

```
