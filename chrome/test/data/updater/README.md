# updater_qualification_app_dmg.crx
To recreate, run:

```
mkdir qual &&
touch qual/.install &&
chmod a+x qual/.install &&
hdiutil create updater_qualification_app_dmg.dmg -fs HFS+ -srcfolder qual &&
xattr -c updater_qualification_app_dmg.dmg
```

Note: -fs HFS+ is required for compatibility with macOS 10.11 and 10.12.

This will give you a DMG file that must then be packaged as a CRX. Signing a
CRX requires access to your brand's signing keys (Google's, Microsoft's, etc).

# updater_qualification_app_exe.crx
To recreate, run:

```
gn args out\qual_app &&
autoninja -C out\qual_app chrome/updater/test/qualification_app
```

Note: your gn args should include:
```
target_cpu="x86"
is_component_build=false
```

We use a 32-bit executable so that it can be used on both 32-bit and 64-bit
bots.

This will give you an EXE file (at out\\qual\_app\\qualification\_app.exe) that
must then be packaged as a CRX. Signing a CRX requires access to your brand's
signing keys (Google's, Microsoft's, etc).
