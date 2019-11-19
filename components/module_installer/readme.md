# Chrome on Android Dynamic Feature Module Installer Backend

This component houses code to install and load Android
[dynamice feature modules](https://developer.android.com/guide/app-bundle). See
the [onboarding guide](../../docs/android_dynamic_feature_modules.md) for how to
create such a feature module in Chrome. Broadly, this component offers two APIs
- _install engine_ to install modules and _module builder_ to set up modules on
first access.

## Install Engine

The install engine is a wrapper around Play Core's
[split install API](https://developer.android.com/guide/app-bundle/playcore)
that performs extra setup such as
[SplitCompat](https://developer.android.com/guide/app-bundle/playcore#access_downloaded_modules),
collects metrics and provides
[fake install](android/java/src/org/chromium/components/module_installer/engine/FakeEngine.java).
You can install a module by name with the following code snippet:

```java
InstallEngine installEngine = new EngineFactory().getEngine();
installEngine.install("foo", success -> {
    // Module installed successfully if |success| is true.
});
```

You can use the install engine on its own but will have to take care of module
setup such as loading native code and resources. To simplify that you can use
the module builder API.

## Module Builder

The module builder simplifies module set up by loading native code and resources
on first module access and determines whether a module is installed. The module
builder uses the install engine in the back. It primarily provides the following
building blocks:

* [`@ModuleInterface`](android/java/src/org/chromium/components/module_installer/builder/ModuleInterface.java)
  to annotate the entry point of your module. Using this with the
  [`module_interface_processor`](android/BUILD.gn) will create a module class
  such as `FooModule` that lets you install and load a module. See
  [`Module`](android/java/src/org/chromium/components/module_installer/builder/Module.java)
  for its interface.

* [`Module`](android/java/src/org/chromium/components/module_installer/builder/Module.java)
  needs to be able to retrieve a
  [`ModuleDescriptor`](android/java/src/org/chromium/components/module_installer/builder/ModuleDescriptor.java)
  implementation for each module via reflection. You can create such an
  implementation with the [`module_desc_java`](android/module_desc_java.gni)
  template.
