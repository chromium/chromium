# Extension API Specifications

[TOC]

## Summary
Extension APIs are specified in JSON or IDL files.  These files
describe the API and the functions, events, types, and properties it contains.
This specification is used for multiple purposes, including setting up the
API’s bindings (the JavaScript end points that are exposed to extensions),
generating strong types and conversion for extension API objects, creating the
public documentation, generating JS externs, and registering extension API
functions.

## Applications

### Bindings
The Extension API specifications generate the JSON objects that describe the
extension APIs.  These JSON objects are then used in the creation of the
extension API bindings to set up the functions, events, and properties that are
exposed to the extensions in JavaScript.  For more information, see the
[bindings documentation](/extensions/renderer/bindings.md).

### Type Generation
Extension API implementations use strong types in the browser
process, generated from the arguments passed by the renderer.  Both the type
definitions themselves and the conversion code to generate between the
serialized base::Values and strong types are generated based on the API
specification.  Additionally, we also generate constants for enums and event
names used in extension APIs.

### Documentation
The public documentation hosted at https://developer.chrome.com/extensions is
largely generated from the API specifications.  Everything from the function
descriptions to parameter ordering to exposed types and enums is generated from
the specification.  Because this becomes our public documentation, special care
should be given to ensure that descriptions for events, functions, properties,
and arguments are clear and well-formed.

### JS Externs
JS externs, which are used with
[Closure Compilation](https://developers.google.com/closure/compiler/), are
generated from the API specifications as well.  These are used both internally
within Chrome (e.g., in our settings WebUI code) as well as externally by
developers writing extensions.

## Structure
The API specification consists of four different sections: functions,
events, types, and properties.  Functions describe the API functions exposed to
extensions.  Events describe the different events that extensions can register
listeners for.  Types are used to define object types to use in either
functions or events.  Finally, properties are used to expose certain properties
on the API object itself.

## Compilation
The extension API compilation is triggered as part of the normal
compilation process (e.g., `ninja -C out/debug chrome`).  The schemas are
"compiled" by the JSON Schema Compiler, which is implemented in
[src/tools/json_schema_compiler](/tools/json_schema_compiler).  As part of this
step, both IDL and JSON extension API specifications are converted to the same
object, which can be output as a JSON object.  This API object is then used in
each of the compilation steps (bindings, type generation, documentation, JS
externs, and function registration).

Compilation steps can be selective, since not all APIs need to be included
in all compilation steps. For instance, some APIs are implemented entirely
in the renderer, and thus do not need to have strong types or function
registration generated. See the JSON Schema Compiler for more information.

## File Location
Extension APIs are defined in two main locations:
[src/chrome/common/extensions/api](/chrome/common/extensions/api/) and
[src/extensions/common/api](/extensions/common/api/). These represent
two different layers in the extensions system. The chrome layer is for
concepts that are purely chrome-related, whereas the extensions layer is for
concepts that may apply to other embedders (such as AppShell or others).

If an API does not rely on Chrome-specific concepts (things like profiles and
tabs are examples of Chrome-specific topics), it should go at the /extensions
layer. The default location is the /extensions layer (though this was added
later, and, as a result, many APIs that wouldn't otherwise need to be defined
at the the /chrome layer, are).

## Adding a New File
To add a new API, create a new IDL or JSON file that defines the API and add it
to the appropriate BUILD.gn groups.  Files can be included in different targets
depending on their platform availability (e.g., an API may be only available on
ChromeOS) and whether they need to be included in various compilation steps
(e.g., an API may not need generated function registration).

### Adding to GN Files

The GN targets that you include the file in depend on which
[applications](#applications) it should be used in. Unfortunately, the targets
aren't as standardized as they should be, so you'll have to trace them to their
GN action usage. (We'd like to fix this at some point.)

The GN actions correspond as below:

`generated_json_strings`: This action generates the bundled JSON strings for
APIs, which are used to set up the extension bindings in the renderer.

`function_registration`: This action generates code to automatically register
the extension function implementations with the ExtensionFunctionRegistry.

`generated_types`: This action generates the strong types used in the browser
process and the conversion to and from these types and `base::Value`s.

Most APIs leverage all of these (and are typically called `schema_sources` or
`schema_files` in the .gni files). Others want to omit certain steps; these
are added to other groups in the .gn files.

If in doubt, you probably want to add yours to the "basic" group. Feel free to
reach out to an extensions OWNER with any questions.

## IDL vs JSON
Extension APIs can be specified in either IDL or JSON.  During
compilation, all files are converted to JSON objects.  The benefit to using a
JSON file is that it is more clear what the JSON object output will look like
(since it’s simply the result of parsing the file).  IDL, on the other hand, is
typically much more readable - especially when an API will have many methods or
long descriptions.  However, IDL is not as fully-featured as JSON in terms of
accepted properties on different nodes.

## Promise Based Function Returns

Extension functions can be made to both accept a callback as a final parameter,
or return a promise if the callback is omitted. For JSON schemas, instead of
specifying the callback as a function at the end of the parameter list, it is
explicitly defined as an asynchronous return using the `returns_async` key on
the function itself. For IDL scheamas, any function where the final argument of
the parameters is a callback type will automatically be considered as supporting
promises, unless the function has the [doesNotSupportPromises="crbug.com/id"]
extended attribute. This extended attribute shouldn't be added to any new APIs
and only exists for a few legacy APIs which can't support promises for various
reasons.
