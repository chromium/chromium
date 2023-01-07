# cros_elements/

This directory contains a UI library for Chrome OS based on
[Material Web Components](https://github.com/material-components/material-components-web-components)

This is currently in a prototyping stage. Contact cros-ui-componentization@google.com for more details.

An example can be seen at
`ash/webui/sample_system_web_app_ui/resources/component_playground.html`
which is served in the Sample System Web App.

## Build Structure

The elements in this directory require tooling that bundles the entire app into a usable output.

See `third_party/material_web_components/build_mwc_app.py` for more details.

Imports from TS must be rewritten into full paths to work with rollup (since we do not have
@rollup/plugin-node-resolve). This is done by the ./rebuild_elements.sh script.

## Updating

As TypeScript is not yet in the toolchain, developers should commit .ts files as well their outputs.
Once TypeScript is integrated into the toolchain, the build tooling will be updated.

For now:
1. Edit TS files
2. Run ./rebuild_elements.sh
3. Commit both TS and JS files
4. Import JS file into client page
