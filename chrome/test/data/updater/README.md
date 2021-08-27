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
